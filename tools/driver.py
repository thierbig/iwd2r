#!/usr/bin/env python3
"""Run GLM workers headless over wave batches.

The worker protocol, a fake worker, the report parser, the credit
estimate, the mechanical checks and the review policy, the ledger
template, the batch loop (`wave.py run`) with its budget stop, 429 park
and resume, and `--clear`.  The driver itself uses no model."""
import argparse
import collections
import contextlib
import datetime
import fcntl
import glob
import hashlib
import json
import importlib.util
import os
import random
import shlex
import shutil
import signal
import re
import queue as queue_mod
import subprocess
import tempfile
import sys
import threading
import time

# api_error: the kind of an API error no retry cures (ruling R46), or None.
Result = collections.namedtuple(
    'Result', 'exit_code report transcript credits rate_limited api_error '
    'timed_out', defaults=(None, False))

# (input, cached, output) credits per 10000 tokens.
FLASH_RATES = (2.3, 0.56, 8.0)
GLM53_RATES = (6.9, 1.7, 24.0)
# Z.ai's rate-limit refusal, in a worker's final JSON result or in the
# transcript's last API-error entry (ruling R41).
RATE_LIMIT_TEXT = 'Request rejected (429)'
# API errors no retry cures: the run halts (ruling R46).
FATAL_API_ERRORS = ('authentication_failed', 'billing_error')
FATAL_API_STATUSES = (401, 402, 403)

VERDICT_RE = re.compile(
    r'^verdict (0[xX][0-9a-fA-F]+) (ours|fork|neither) case (\S+)$', re.IGNORECASE)
UNDECIDED_RE = re.compile(
    r'^undecided\s+(0[xX][0-9a-fA-F]+)\s+(fork|neither)\s+(\S.*)$', re.IGNORECASE)
KEPT_RE = re.compile(
    r'^kept\s+(0[xX][0-9a-fA-F]+)\s+(\S.*)$', re.IGNORECASE)
CANDIDATE_RE = re.compile(
    r'^candidate\s+(0[xX][0-9a-fA-F]+)\s+(fork|neither)\s+(\S.*)$', re.IGNORECASE)
IMPORTED_RE = re.compile(r'^imported\s+(0[xX][0-9a-fA-F]+)\b', re.IGNORECASE)
MARKER_RE = re.compile(r'^(?:[-*]|\d+\.)\s+')


class ReportError(ValueError):
    pass


def parse_report(text):
    """Parse the '## Verdicts' section into {addr: (side, case)}.

    Addresses are normalised to '0x' + uppercase hex without leading zeros."""
    out = {}
    in_section = False
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if line.startswith('#'):
            in_section = line.lstrip('#').strip().lower() == 'verdicts'
            continue
        if not in_section or not line:
            continue
        if re.fullmatch(r'```[^`]*', line):
            continue
        cand = MARKER_RE.sub('', line, count=1).strip().strip('`').strip()
        m = VERDICT_RE.match(cand)
        if not m:
            # Prose that names no address and does not try to be a verdict
            # ("All 20 rows are decided; there is no ## Undecided section.")
            # decides nothing and is skipped: a mangled verdict line always
            # carries its address or the word verdict, and those still fail.
            if not re.search(r'0[xX][0-9a-fA-F]+', cand) and \
                    not cand.lower().startswith('verdict'):
                continue
            raise ReportError('line %d: malformed verdict line: %r' % (n, raw))
        addr = '0x%X' % int(m.group(1), 16)
        side, case = m.group(2).lower(), m.group(3)
        if addr in out:
            raise ReportError('line %d: duplicate verdict for %s' % (n, addr))
        out[addr] = (side, case)
    return out


def _rates(model):
    """Rates for a model name, or None for a non-glm model (skipped)."""
    model = re.sub(r'\[1m\]$', '', model or '')
    if 'flash' in model:
        return FLASH_RATES
    if model.startswith('glm'):
        return GLM53_RATES
    return None


def credits_of(path):
    """Sum usage in a transcript, once per message.id (last line wins).
    Lines are data, never executed."""
    by_id = {}
    anon = []
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            try:
                obj = json.loads(line)
            except ValueError:
                continue
            msg = obj.get('message') if isinstance(obj, dict) else None
            usage = msg.get('usage') if isinstance(msg, dict) else None
            if not isinstance(usage, dict):
                continue
            mid = msg.get('id')
            if isinstance(mid, str) and mid:
                by_id[mid] = (msg.get('model'), usage)
            else:
                anon.append((msg.get('model'), usage))
    total = 0.0
    for model, usage in list(by_id.values()) + anon:
        rates = _rates(model)
        if rates is None:
            continue
        inp = (usage.get('input_tokens') or 0) + \
            (usage.get('cache_creation_input_tokens') or 0)
        total += (inp * rates[0]
                  + (usage.get('cache_read_input_tokens') or 0) * rates[1]
                  + (usage.get('output_tokens') or 0) * rates[2]) / 10000
    return total


def subagent_transcripts(path):
    """The transcripts of the subagents a session ran:
    <transcript dir>/<session id>/subagents/*.jsonl, sorted; [] when none
    (ruling R71)."""
    if not path:
        return []
    session = os.path.splitext(path)[0]
    return sorted(glob.glob(os.path.join(glob.escape(session), 'subagents',
                                         '*.jsonl')))


def usage_all(path):
    """usage_of summed over the transcript and its subagents'."""
    tokens, calls = usage_of(path)
    for sub in subagent_transcripts(path):
        try:
            t, c = usage_of(sub)
        except OSError:
            continue
        for k in tokens:
            tokens[k] += t[k]
        calls += c
    return tokens, calls


def usage_of(path):
    """(tokens, tool_calls) for a transcript (ruling R61): tokens is
    {input, cached, output} summed over the transcript's assistant
    messages once per message.id (last line wins, as credits_of), input
    counting cache creation as credits_of does; tool_calls counts the
    distinct tool_use blocks.  Any model counts: credits stay GLM-only,
    these figures do not.  Lines are data, never executed."""
    by_id = {}
    anon = []
    calls, anon_calls = set(), 0
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            try:
                obj = json.loads(line)
            except ValueError:
                continue
            msg = obj.get('message') if isinstance(obj, dict) else None
            if not isinstance(msg, dict):
                continue
            content = msg.get('content')
            if obj.get('type') == 'assistant' and isinstance(content, list):
                for b in content:
                    if isinstance(b, dict) and b.get('type') == 'tool_use':
                        if isinstance(b.get('id'), str) and b['id']:
                            calls.add(b['id'])
                        else:
                            anon_calls += 1
            usage = msg.get('usage')
            if not isinstance(usage, dict):
                continue
            mid = msg.get('id')
            if isinstance(mid, str) and mid:
                by_id[mid] = usage
            else:
                anon.append(usage)
    tokens = {'input': 0, 'cached': 0, 'output': 0}
    for usage in list(by_id.values()) + anon:
        tokens['input'] += (usage.get('input_tokens') or 0) + \
            (usage.get('cache_creation_input_tokens') or 0)
        tokens['cached'] += usage.get('cache_read_input_tokens') or 0
        tokens['output'] += usage.get('output_tokens') or 0
    return tokens, len(calls) + anon_calls


def _texts(content):
    """The text blocks of a message's content (a string or a block list)."""
    if isinstance(content, str):
        return [content]
    if not isinstance(content, list):
        return []
    return [b['text'] for b in content if isinstance(b, dict)
            and b.get('type') == 'text' and isinstance(b.get('text'), str)]


def fatal_api_error(entry):
    """The kind of an auth or billing error in a dict carrying Claude
    Code's API-error keys (`error`, `apiErrorStatus`), else None."""
    error = entry.get('error')
    if error in FATAL_API_ERRORS:
        return error
    status = entry.get('apiErrorStatus')
    if status in FATAL_API_STATUSES:
        return 'status %d' % status
    return None


def scan_transcript(path):
    """(kind, ran) for a worker transcript.  kind is what the LAST
    API-error entry says when no successful assistant turn follows it:
    'rate_limit' (apiErrorStatus 429 or error rate_limit, RATE_LIMIT_TEXT
    in its own text as the fallback), an auth or billing kind, or None
    (rulings R41, R44, R46).  ran is whether any successful, non-synthetic
    assistant turn exists: the worker really ran (R46).  The API-error
    entry is the synthetic assistant line Claude Code writes with
    isApiErrorMessage; tool_use and tool_result content, file text and
    the worker's prose are never read.  Lines are parsed as JSON data,
    never executed."""
    kind, ran = None, False
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            try:
                obj = json.loads(line)
            except ValueError:
                continue
            if not isinstance(obj, dict) or obj.get('type') != 'assistant':
                continue
            msg = obj.get('message')
            msg = msg if isinstance(msg, dict) else {}
            if obj.get('isApiErrorMessage') is True:
                if (obj.get('apiErrorStatus') == 429
                        or obj.get('error') == 'rate_limit'
                        or any(RATE_LIMIT_TEXT in t
                               for t in _texts(msg.get('content')))):
                    kind = 'rate_limit'
                else:
                    kind = fatal_api_error(obj)
            elif msg.get('model') != '<synthetic>':
                kind, ran = None, True   # a successful turn: retried past
    return kind, ran


def worker_ran(res):
    """True when a run's transcript holds a successful, non-synthetic
    assistant turn: the worker really ran (the breaker, ruling R46)."""
    if res.transcript is None:
        return False
    try:
        return scan_transcript(res.transcript)[1]
    except OSError:
        return False


def project_slug(cwd):
    return re.sub(r'[^A-Za-z0-9]', '-', os.path.abspath(cwd))


class FakeWorker:
    def __init__(self, report, exit_code=0, rate_limited=False):
        self.report = report
        self.exit_code = exit_code
        self.rate_limited = rate_limited

    def run(self, brief, cwd):
        return Result(self.exit_code, self.report, None, None, self.rate_limited)


# The worker's environment (ruling R64).  A driver launched from inside a
# Claude Code session must not let its workers attach to that session, and
# a plain-claude worker must not inherit the GLM wrapper's endpoint.
SESSION_VARS = ('CLAUDE_CODE_CHILD_SESSION', 'CLAUDE_CODE_ENTRYPOINT',
                'CLAUDE_CODE_EXECPATH', 'CLAUDE_PID', 'CLAUDE_EFFORT',
                'CLAUDECODE')
SESSION_PREFIXES = ('CLAUDE_CODE_MESSAGING_', 'CLAUDE_CODE_SESSION_')
# The launching shell's AoE identity (ruling R71): the user's hooks call
# $AOE_HOOK_BIN whenever AOE_INSTANCE_ID is set, which would re-bind the
# coordinator's session to the worker.  Dropped for every wrapper.
AOE_PREFIX = 'AOE_'
# What the driver's own `aoe send` must not inherit: the coordinator's
# Claude Code session.  AOE_* stays (aoe finds its session and profile
# through it; `aoe send --help` names no other way than --profile).
SEND_DROP = ('CLAUDECODE', 'CLAUDE_PID', 'CLAUDE_EFFORT')
SEND_DROP_PREFIXES = ('CLAUDE_CODE_',)


def send_env(environ):
    """environ for `aoe send`: without the Claude Code session variables."""
    return {k: v for k, v in environ.items()
            if k not in SEND_DROP and not k.startswith(SEND_DROP_PREFIXES)}

GLM_VARS = ('ANTHROPIC_BASE_URL', 'ANTHROPIC_AUTH_TOKEN',
            'CLAUDE_CODE_AUTO_COMPACT_WINDOW', 'API_TIMEOUT_MS')
GLM_PREFIXES = ('ANTHROPIC_DEFAULT_', 'CLAUDE_GLM_', 'CLAUDE_CODE_SUBAGENT_MODEL')


def _glm_config_dir():
    return os.environ.get('CLAUDE_GLM_CONFIG_DIR') or \
        os.path.expanduser('~/.claude-glm')


def _in_glm_config(path):
    glm = os.path.realpath(_glm_config_dir())
    real = os.path.realpath(path)
    return real == glm or real.startswith(glm + os.sep)


def worker_env(environ, glm):
    """environ without the variables that tie a worker to the launching
    session; for a wrapper other than claude-glm (glm False) also without
    the GLM wrapper's endpoint, token and model settings.  Nothing is
    added but CLAUDE_CODE_DISABLE_AUTO_MEMORY=1."""
    env = dict(environ)
    # Worktrees share the project's auto-memory folder; a worker must not
    # write to it (ruling R72).
    env['CLAUDE_CODE_DISABLE_AUTO_MEMORY'] = '1'
    for k in list(env):
        drop = (k in SESSION_VARS or k.startswith(SESSION_PREFIXES)
                or k.startswith(AOE_PREFIX))
        if not glm:
            drop = (drop or k in GLM_VARS or k.startswith(GLM_PREFIXES)
                    or (k == 'CLAUDE_CONFIG_DIR' and _in_glm_config(env[k])))
        if drop:
            del env[k]
    return env


DEFAULT_TIMEOUT = 10800   # seconds a worker or reviewer run may take (R71)


# Process groups of the live worker and reviewer runs (pgid = child pid), so
# that stopping the driver stops them (ruling R72).
_LIVE = set()
# Re-entrant: the stop handler runs on a thread that may hold it already.
_LIVE_LOCK = threading.RLock()
# The pids of the probe.py launches among them (under _LIVE_LOCK); once a stop
# has set _STOPPED they are kept, so the stop can tell a probe was killed.
_PROBE_PIDS = set()


# Set (under _LIVE_LOCK) once a stop has killed the live groups: a worker
# forked after that kill, before it joined _LIVE, is killed on joining.
_STOPPED = threading.Event()


class _Stop(BaseException):
    """The driver was told to stop (SIGTERM, SIGINT)."""


def kill_live_groups():
    """SIGKILL every live worker process group; the pgids killed."""
    with _LIVE_LOCK:
        groups = sorted(_LIVE)
        _STOPPED.set()
    for pgid in groups:
        try:
            os.killpg(pgid, signal.SIGKILL)
        except OSError:
            pass
    return groups


def _run_group(argv, timeout=None, probe=False, **kw):
    """subprocess.run in a session of its own, so that a timeout kills the
    whole process group (the worker's children too), then raises
    TimeoutExpired.  probe=True records the pid in _PROBE_PIDS."""
    proc = subprocess.Popen(argv, start_new_session=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            **{k: v for k, v in kw.items()
                               if k not in ('capture_output',)})
    with _LIVE_LOCK:
        _LIVE.add(proc.pid)
        if probe:
            _PROBE_PIDS.add(proc.pid)
        if _STOPPED.is_set():      # forked after the stop's kill: not missed
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except OSError:
                pass
    try:
        out, err = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except OSError:
            proc.kill()
        proc.communicate()
        raise
    except BaseException:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except OSError:
            pass
        proc.wait()
        raise
    finally:
        with _LIVE_LOCK:
            _LIVE.discard(proc.pid)
            if not _STOPPED.is_set():
                _PROBE_PIDS.discard(proc.pid)
    return subprocess.CompletedProcess(argv, proc.returncode, out, err)


def _run_probe(argv, timeout=None, **kw):
    """frida_run's `run` in the real driver: a probe.py launch is a live
    process group like a worker's, so a stop (kill_live_groups) ends it; a
    probe killed by the stop raises _Stop in its batch thread instead of
    coming home as a failed scenario.  Killing the group does not close the
    Windows Sandbox the probe started (the box runs to its in-box timeout);
    the stop taskkills it (Driver._close_sandbox_after_stop), and only when
    a probe was among the groups it killed (_PROBE_PIDS)."""
    done = _run_group(argv, timeout=timeout, probe=True, **kw)
    if _STOPPED.is_set() and done.returncode < 0:
        raise _Stop()
    return done


class HeadlessWorker:
    def __init__(self, model='glm-5.3-flash[1m]', max_turns=400,
                 wrapper='claude-glm', config_dir=None, run=None,
                 timeout=DEFAULT_TIMEOUT):
        self.model = model
        self.max_turns = max_turns
        self.wrapper = wrapper
        self.timeout = timeout
        self._run = _run_group if run is None else run
        if config_dir is None:
            if os.path.basename(wrapper) == 'claude-glm':
                config_dir = (os.environ.get('CLAUDE_GLM_CONFIG_DIR')
                              or os.path.expanduser('~/.claude-glm'))
            else:
                # A plain claude keeps its transcripts in ~/.claude (or
                # CLAUDE_CONFIG_DIR), never in the GLM wrapper's folder.
                cfg = os.environ.get('CLAUDE_CONFIG_DIR')
                if not cfg or _in_glm_config(cfg):
                    cfg = os.path.expanduser('~/.claude')
                config_dir = cfg
        self.config_dir = config_dir

    def command(self, brief):
        argv = [self.wrapper, '-p', brief, '--output-format', 'json',
                '--dangerously-skip-permissions', '--max-turns', str(self.max_turns)]
        glm = os.path.basename(self.wrapper) == 'claude-glm'
        env = worker_env(os.environ, glm)
        if glm:
            env['CLAUDE_GLM_MODEL'] = self.model
        else:
            # A plain `claude` takes the model as a flag, and its
            # transcripts are under ~/.claude/projects (ruling R23).
            argv += ['--model', self.model]
            # Worker isolation (ruling R71): only the project's and the
            # local settings, no user hooks or MCP servers.
            argv += ['--setting-sources', 'project,local', '--strict-mcp-config']
            env['ENABLE_CLAUDEAI_MCP_SERVERS'] = '0'
        return argv, env

    def transcript_path(self, cwd, session_id):
        return os.path.join(self.config_dir, 'projects', project_slug(cwd),
                            session_id + '.jsonl')

    def run(self, brief, cwd):
        argv, env = self.command(brief)
        try:
            proc = self._run(argv, cwd=cwd, env=env, capture_output=True,
                             text=True, stdin=subprocess.DEVNULL,
                             timeout=self.timeout)
        except subprocess.TimeoutExpired:
            return Result(124, '', None, None, False, None, True)
        report, session_id = '', None
        exit_code = proc.returncode
        rate_limited, api_error = False, None
        try:
            data = json.loads(proc.stdout)
            if isinstance(data, dict):
                report = data.get('result') or ''
                session_id = data.get('session_id')
                # The final result says so: an error result (is_error, or
                # text that starts `API Error`) with the 429 text.  A
                # report that quotes the phrase is not one.
                rate_limited = (
                    isinstance(report, str) and RATE_LIMIT_TEXT in report
                    and (data.get('is_error') is True
                         or report.lstrip().startswith('API Error')))
                api_error = fatal_api_error(data)
        except ValueError:
            report = proc.stdout or ''
            exit_code = exit_code or 1
        transcript = credits = None
        if isinstance(session_id, str) and re.fullmatch(r'[A-Za-z0-9_-]+', session_id):
            path = self.transcript_path(cwd, session_id)
            if os.path.isfile(path):
                transcript = path
                credits = credits_of(path)
                kind, _ran = scan_transcript(path)
                if kind == 'rate_limit':
                    rate_limited = True
                elif kind is not None and api_error is None:
                    api_error = kind
        return Result(exit_code, report, transcript, credits, rate_limited,
                      api_error)


_TOOLS = {}


def _tool(name):
    """tools/<name>.py loaded by path: under -I tools/ is not on sys.path."""
    if name not in _TOOLS:
        spec = importlib.util.spec_from_file_location(
            'driver_' + name, os.path.join(
                os.path.dirname(os.path.abspath(__file__)), name + '.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _TOOLS[name] = module
    return _TOOLS[name]


def _vanilla_exe():
    return _tool('oracle').VANILLA_EXE


def _parity_python():
    """As tools/gates.sh: the parity venv when executable, else python3 -I."""
    home = os.environ.get('IWD2R_HOME') or os.path.expanduser('~/.local/share/iwd2r')
    venv = os.path.join(home, 'parity-venv', 'bin', 'python')
    if os.access(venv, os.X_OK):
        return [venv]
    return ['python3', '-I']


def _tail(proc, n=5):
    text = ((proc.stdout or '') + (proc.stderr or '')).strip()
    return ' | '.join(text.splitlines()[-n:])

# -- the worker's transcript (Task 16) --------------------------------------

# The runners a worker may launch the game, a sandbox or a trace with.
HOUSE_RUNNERS = ('tools/play.py', 'tools/gltrace.py')
READ_LIMIT = 200      # lines of a covered source file one read may take
_SEGMENT = re.compile(r'&&|\|\||;|\n')
_WSO_REF = re.compile(r"(?:(?<=[\s'\"=:.^~@])|(?<=remotes/)|(?<=heads/))wso/")
# The git subcommands that put the fork tree on disk: a wso/ ref named with
# one of them is fatal (Task 33). The read-only subcommands, and `branch`
# without a write flag, only read the ref.
_WSO_WRITE_SUBS = frozenset((
    'checkout', 'switch', 'merge', 'rebase', 'cherry-pick', 'reset',
    'restore', 'read-tree', 'archive', 'worktree', 'pull', 'fetch',
    'submodule', 'am', 'apply', 'revert'))
_WSO_READ_SUBS = frozenset((
    'rev-parse', 'log', 'show', 'diff', 'ls-tree', 'cat-file',
    'merge-base', 'name-rev', 'describe', 'for-each-ref'))
_SED_SPAN = re.compile(r"(\d+),(\d+)p")
_LAUNCH = re.compile(r'\.wsb\b|WindowsSandbox|Start-Process', re.I)
# A command that sets the desktop override: an assignment (leading word,
# export, env), os.environ or putenv in python -c, setenv, `set`.
_DESKTOP_SET = re.compile(
    r'IWD2R_DESKTOP_OK\s*='
    r'|(?:os\.environ|putenv|setenv|export|\$env:|\bset\b)[^;\n]*IWD2R_DESKTOP_OK')
_LAUNCH_SHELLS = ('cmd', 'powershell', 'pwsh', 'explorer', 'wsl', 'bash', 'sh')


def _is_runner(text):
    for r in HOUSE_RUNNERS:
        if re.search(r'\bpython3\s+-I\s+(?:\S*/)?%s\b' % re.escape(r), text):
            return True
    return False


def _words(stage):
    """(command word, its arguments): leading VAR=value words and sudo
    skipped; the command word without its directory or .exe."""
    try:
        toks = shlex.split(stage)
    except ValueError:
        toks = stage.split()
    while toks and (re.match(r'^[A-Za-z_]\w*=', toks[0]) or toks[0] == 'sudo'):
        toks = toks[1:]
    if not toks:
        return '', []
    cw = os.path.basename(toks[0].replace('\\', '/')).lower()
    if cw.endswith('.exe'):
        cw = cw[:-4]
    return cw, toks[1:]


def _bound(cw, args):
    """The most lines a head, tail or sed -n stage passes on, or None."""
    if cw in ('head', 'tail'):
        for i, a in enumerate(args):
            m = re.match(r'^-n(\d+)$', a) or re.match(r'^-(\d+)$', a)
            if m:
                return int(m.group(1))
            if a == '-n' and i + 1 < len(args) and args[i + 1].isdigit():
                return int(args[i + 1])
        return 10
    if cw == 'sed' and '-n' in args:
        for a in args:
            m = _SED_SPAN.search(a)
            if m:
                return int(m.group(2)) - int(m.group(1)) + 1
    return None


def _short(text, n=100):
    text = ' '.join(str(text).split())
    return text if len(text) <= n else text[:n - 3] + '...'


def _tool_calls(path):
    """(name, input) of every tool_use block of the assistant messages in a
    Claude Code JSONL transcript; unparseable lines are skipped."""
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            try:
                obj = json.loads(line)
            except ValueError:
                continue
            msg = obj.get('message') if isinstance(obj, dict) else None
            content = msg.get('content') if isinstance(msg, dict) else None
            if not isinstance(content, list):
                continue
            for block in content:
                if isinstance(block, dict) and block.get('type') == 'tool_use' \
                        and isinstance(block.get('input'), dict):
                    yield block.get('name'), block['input']


_HISTORY_VERBS = ('reset', 'rebase', 'revert', 'restore', 'clean', 'stash',
                  'filter-repo', 'push', 'update-ref', 'update-index')


# No real branch is named for these (the batch's own path prefixes).
_BATCH_DIRS = ('src', 'compat', 'docs', 'oracle', 'tools', 'tests')


_HEREDOC = re.compile(r"""<<-?[ \t]*(?:'([^'\n]*)'|"([^"\n]*)"|([^\s;&|<>()]+))""")


# A heredoc fed to one of these runs its body as commands.
_SHELLS = ('bash', 'sh', 'zsh', 'dash', 'ksh', 'wsl', 'pwsh', 'powershell')


def _fallback_split(text, seps):
    return text.split('|') if seps == ('|',) else _SEGMENT.split(text)


def _feeds_shell(stage):
    """True when a word of a pipe stage names a shell (the command word,
    or after env, exec, nohup and the like)."""
    cw, args = _words(stage)
    for w in [cw] + args:
        w = os.path.basename(w.replace('\\', '/')).lower()
        if (w[:-4] if w.endswith('.exe') else w) in _SHELLS:
            return True
    return False


def _split_unquoted(text, seps):
    """text split at any of seps (longest first) outside quotes; a
    backslash escapes the next character, an unquoted # at the start of a
    word is a comment to the end of the line, and the body of a heredoc
    (<<WORD, <<-WORD, <<'WORD') is data up to its delimiter line, unless
    a stage of the pipeline that opened it names a shell (_feeds_shell,
    before or after the <<): that body is commands and is split the same
    way.  Fails closed: a quote or heredoc left open at the end falls
    back to _SEGMENT."""
    parts, cur, quote, i, pending = [], [], None, 0, []
    while i < len(text):
        ch = text[i]
        if quote:
            cur.append(ch)
            if ch == '\\' and quote == '"' and i + 1 < len(text):
                i += 1
                cur.append(text[i])
            elif ch == quote:
                quote = None
        elif ch == '\\' and i + 1 < len(text):
            cur.append(ch + text[i + 1])
            i += 1
        elif ch in '\'"':
            quote = ch
            cur.append(ch)
        elif ch == '#' and (not cur or cur[-1][-1:] in ' \t'):
            j = text.find('\n', i)
            i = len(text) if j < 0 else j
            continue
        elif ch == '\n' and pending:
            parts.append(''.join(cur))
            cur = []
            delims, pending = pending, []
            i += 1
            for d, idx in delims:
                # the whole pipeline of the segment that opened it (slots
                # stay valid, bodies are appended only after this newline);
                # a trailing | continues it past the body: fail closed
                run = parts[idx].rstrip().endswith('|') or any(
                    _feeds_shell(st)
                    for st in _split_unquoted(parts[idx], ('|',)))
                found, body = False, []
                while i < len(text) and not found:
                    j = text.find('\n', i)
                    line = text[i:len(text) if j < 0 else j]
                    i = len(text) if j < 0 else j + 1
                    found = line.strip() == d
                    if not found:
                        body.append(line)
                if not found:           # no delimiter line: fail closed
                    return _fallback_split(text, seps)
                if run:                 # fed to a shell: the body is commands
                    parts += _split_unquoted('\n'.join(body), seps)
            continue
        elif text.startswith('<<', i) and not text.startswith('<<<', i) \
                and _HEREDOC.match(text, i):
            m = _HEREDOC.match(text, i)
            d = next(g for g in m.groups() if g is not None)
            pending.append((re.sub(r'[\\\'"]', '', d) if m.group(3) else d,
                            len(parts)))
            cur.append(text[i:m.end()])
            i = m.end()
            continue
        else:
            sep = next((x for x in seps if text.startswith(x, i)), None)
            if sep:
                parts.append(''.join(cur))
                cur = []
                i += len(sep) - 1
            else:
                cur.append(ch)
        i += 1
    if quote or pending:
        return _fallback_split(text, seps)
    parts.append(''.join(cur))
    return parts


def _history_rewrite(args, covered=(), tracked=None):
    """True when the arguments of a `git` command (after the command word)
    rewrite or discard history or work: reset, rebase, revert, restore,
    clean, stash (except the read-only `stash list` / `stash show`),
    filter-repo, push, update-ref, update-index (it can hide local
    changes behind a skip-worktree/assume-unchanged flag), commit --amend,
    branch -d/-D, reflog expire, gc --prune, and a checkout that is not a plain
    branch switch (`--`, -f, -B, `.`, a path after a start point, or one
    argument that names a covered file, looks like a path or starts with a
    batch directory), branch -f/-M/-C and switch -C/--force-create.
    add, commit, switch, checkout -b / <branch> and the read-only
    commands are not.  `tracked(name)`, when given, says whether a name is a
    file tracked in the worktree: one checkout argument naming one is a
    path restore, not a branch switch."""
    rest = list(args)
    while rest and rest[0].startswith('-'):      # git -C dir / -c k=v / --x
        rest = rest[2:] if rest[0] in ('-C', '-c') else rest[1:]
    if not rest:
        return False
    sub, rest = rest[0], rest[1:]
    if sub == 'stash':
        # `git stash list` / `git stash show` only read the stash; every
        # other form (push, pop, apply, drop, clear, a bare `git stash`)
        # moves work around
        pos = [a for a in rest if not a.startswith('-')]
        return not (pos and pos[0] in ('list', 'show'))
    if sub in _HISTORY_VERBS:
        return True
    if sub == 'commit':
        return any(a == '--amend' for a in rest)
    if sub == 'branch':
        return any(a in ('--delete', '--force')
                   or re.fullmatch(r'-[a-zA-Z]*[dDfMC][a-zA-Z]*', a)
                   for a in rest)
    if sub == 'reflog':
        return 'expire' in rest or 'delete' in rest
    if sub == 'gc':
        return any(a.startswith('--prune') for a in rest)
    if sub == 'switch':
        return any(a in ('--force', '--discard-changes', '--force-create')
                   or re.fullmatch(r'-[a-zA-Z]*[fC][a-zA-Z]*', a)
                   for a in rest)
    if sub == 'checkout':
        if '--' in rest or any(
                a in ('--force', '--patch', '--ours', '--theirs')
                or re.fullmatch(r'-[a-zA-Z]*[fpB][a-zA-Z]*', a) for a in rest):
            return True
        if '-b' in rest:
            return False
        pos = [a for a in rest if not a.startswith('-')]
        if len(pos) != 1:
            return len(pos) > 1
        a = pos[0]
        return (a == '.' or a.startswith(('./', '../'))
                or any(ch in a for ch in '*?[')
                or a.split('/')[0] in _BATCH_DIRS
                or any(a == c or a.endswith('/' + c) for c in covered)
                or bool(tracked and tracked(a)))
    return False


def _is_amend(args):
    """True for `git commit --amend`: a rewrite of a commit, not a loss of work."""
    rest = list(args)
    while rest and rest[0].startswith('-'):
        rest = rest[2:] if rest[0] in ('-C', '-c') else rest[1:]
    return bool(rest) and rest[0] == 'commit' and '--amend' in rest[1:]


def _wso_ref_finding(args, short):
    """The finding for a `git` command naming a wso/ ref: ('wso', ...) when
    the subcommand is one of _WSO_WRITE_SUBS, which put the fork tree on
    disk; ('wso-read', ...) for the read-only subcommands and a `branch`
    without -f/-M/-C/-d/-D (a ledger line, not a problem: i020 lost a review
    over a fatal `git rev-parse wso/main`); None for anything else — a
    `branch` with a write flag names the ref only as history rewrite
    (_history_rewrite catches it)."""
    rest = list(args)
    while rest and rest[0].startswith('-'):      # git -C dir / -c k=v / --x
        rest = rest[2:] if rest[0] in ('-C', '-c') else rest[1:]
    if not rest:
        return None
    sub, rest = rest[0], rest[1:]
    if sub in _WSO_WRITE_SUBS:
        return ('wso', 'git command naming a wso/ ref (%s): read the fork '
                'only at the pinned sha, `git show <sha>:<path>`' % short)
    if sub in _WSO_READ_SUBS or (sub == 'branch' and not any(
            a in ('--delete', '--force')
            or re.fullmatch(r'-[a-zA-Z]*[dDfMC][a-zA-Z]*', a) for a in rest)):
        return ('wso-read', 'transcript: read a wso/ ref (%s)' % short)
    return None


def transcript_findings(transcript_path, covered_files, tracked=None):
    """[(kind, problem, file)] for the forbidden tool calls in a worker's
    transcript.  kind 'wso' (a git subcommand that puts a wso/ ref on disk,
    _WSO_WRITE_SUBS; the pinned sha is allowed), 'wso-read' (a git command
    that only reads a wso/ ref, _WSO_READ_SUBS or a plain `branch`; a ledger
    line, not a problem), 'launch' (a .wsb, WindowsSandbox or Start-Process
    launch outside HOUSE_RUNNERS), 'read' (a whole-file read of a covered
    source file: no Read limit or one over READ_LIMIT; cat, a sed -n span
    or head/tail -n over READ_LIMIT; file is the covered file),
    'history' (a git command that rewrites history or discards work, see
    _history_rewrite; terminal like launch; file is 'amend:<command>' for a
    commit --amend, which the driver forgives when only the worker's own commits
    moved; deliberately only that verb: rebase, branch -f, push and update-ref
    can move or publish commits the worker does not own, so they stay fatal) or 'unreadable'.  cat and the launch words count only as a
    command word, and a cat piped into a bounded head/tail/sed is bounded."""
    covered = [c for c in covered_files if c]
    out = []
    calls = []
    for path in [transcript_path] + subagent_transcripts(transcript_path):
        try:
            calls += list(_tool_calls(path))
        except OSError as e:
            return [('unreadable', 'transcript %s could not be read (%s); '
                     'the rules about tool calls cannot be checked'
                     % (path, e), None)]
    for name, inp in calls:
        if name == 'Bash' and _DESKTOP_SET.search(str(inp.get('command', ''))):
            out.append(('launch', 'sets IWD2R_DESKTOP_OK (%s): only a person '
                        'at the PC sets it' % _short(inp.get('command', '')),
                        None))
        if name == 'Read':
            fp = str(inp.get('file_path', ''))
            hit = [c for c in covered if fp == c or fp.endswith('/' + c)]
            limit = inp.get('limit')
            if hit and (not isinstance(limit, int) or limit > READ_LIMIT):
                out.append(('read', 'Read of %s with %s: read a covered '
                            'source file in slices of at most %d lines '
                            '(offset and limit)' % (
                                hit[0], 'no limit' if not isinstance(limit, int)
                                else 'limit %d' % limit, READ_LIMIT), hit[0]))
        elif name == 'Bash':
            for seg in _split_unquoted(str(inp.get('command', '')),
                                       ('&&', '||', ';', '\n')):
                seg = seg.strip()
                if not seg:
                    continue
                stages = [_words(st) for st in _split_unquoted(seg, ('|',))]
                for i, (cw, args) in enumerate(stages):
                    text = ' '.join(args)
                    if cw == 'git' and _WSO_REF.search(' ' + text):
                        hit = _wso_ref_finding(args, _short(seg))
                        if hit:
                            out.append(hit + (None,))
                    if cw == 'git' and _history_rewrite(args, covered, tracked):
                        out.append(('history', 'history rewrite in a worker '
                                    '(%s): workers only add and commit'
                                    % _short(seg),
                                    'amend:' + _short(seg) if _is_amend(args) else None))
                    launch = cw in ('windowssandbox', 'start-process', 'start') \
                        or (cw in _LAUNCH_SHELLS and _LAUNCH.search(text))
                    if launch and not _is_runner(seg):
                        out.append(('launch', 'launch outside a house runner '
                                    '(%s): launch only through `python3 -I %s`'
                                    % (_short(seg), '` or `python3 -I '.join(
                                        HOUSE_RUNNERS)), None))
                    for c in covered:
                        if not any(a == c or a.endswith('/' + c) for a in args):
                            continue
                        why = None
                        if cw == 'cat':
                            later = [_bound(*st) for st in stages[i + 1:]]
                            if not any(b is not None and b <= READ_LIMIT
                                       for b in later):
                                why = 'cat of %s' % c
                        else:
                            b = _bound(cw, args)
                            if b is not None and b > READ_LIMIT:
                                why = '%s of %s over %d lines' % (cw, c, READ_LIMIT)
                        if why:
                            out.append(('read', '%s: read a covered source '
                                        'file in slices of at most %d lines'
                                        % (why, READ_LIMIT), c))
    return out


def transcript_checks(transcript_path, covered_files, tracked=None):
    """One problem per forbidden worker tool call (transcript_findings); a
    wso-read (a git read of a wso/ ref) is a ledger line, not a problem."""
    return [text for kind, text, _f in
            transcript_findings(transcript_path, covered_files, tracked)
            if kind != 'wso-read']


# The paths a batch may change (ruling R71): anything else on the branch is
# a check problem, tools/, tests/, .claude/, .mcp.json and docs/wso/ included
# (but for the import parked rows, one file per row, that the worker writes).
ALLOWED_PATHS = ('src/', 'compat/', 'docs/evidence/', 'docs/MAP.md',
                 'oracle/cases/', 'oracle/CMakeLists.txt',
                 'docs/wso/import-parked/',
                 # an imported function's placeholder label goes with the
                 # import (import template, rule 2): src/ now defines it
                 'tools/ghidra/extra_labels.txt')

# The driver's files in a batch worktree (Task 33): a dirty one is restored
# before the checks (a selfcheck that could not find the live state used to
# copy it in), and one in the worker's own diff is a problem to revert.
DRIVER_OWNED = ('docs/wso/wave-state.json', 'docs/wso/import-state.json',
                'docs/wso/probe-state.json', 'docs/wso/probe-list.json',
                'docs/wso/coverage.json', 'docs/wso/triage.json',
                'docs/wso/xrec-report.md', 'docs/MAP.md')


# The worker's draft of its report (Task 24): written in the worktree, checked
# by `wave.py selfcheck`, ignored by git, never an uncommitted path.
REPORT_DRAFT = '.report.md'


def _path_allowed(path):
    return any(path == a or (a.endswith('/') and path.startswith(a))
               for a in ALLOWED_PATHS)


def _git_paths(run, worktree, args, parse):
    """(paths, problem) from a git command run in the worktree."""
    proc = run(['git', *args], cwd=worktree, capture_output=True, text=True)
    if proc.returncode != 0:
        return [], 'git %s failed (exit %s): %s' % (
            ' '.join(args), proc.returncode, _tail(proc))
    return [parse(l) for l in (proc.stdout or '').splitlines() if l.strip()], None


def parse_undecided(text):
    """Parse the '## Undecided' section into {addr: (side, reason)} (ruling
    R73): rows the worker's reading says are fork or neither but that no
    probe, replay or trace case it can run decides.  Same tolerance as
    parse_report; a missing section is {}."""
    out = {}
    in_section = False
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if line.startswith('#'):
            in_section = line.lstrip('#').strip().lower() == 'undecided'
            continue
        if not in_section or not line:
            continue
        if re.fullmatch(r'```[^`]*', line):
            continue
        cand = MARKER_RE.sub('', line, count=1).strip().strip('`').strip()
        m = UNDECIDED_RE.match(cand)
        if not m:
            # The same tolerance as parse_report: prose that names no address
            # and does not try to be an undecided line ("(none)", "no rows,
            # all 20 decided") is skipped; a mangled line still fails.
            if not re.search(r'0[xX][0-9a-fA-F]+', cand) and \
                    not cand.lower().startswith('undecided'):
                continue
            raise ReportError('line %d: malformed undecided line: %r' % (n, raw))
        addr = '0x%X' % int(m.group(1), 16)
        if addr in out:
            raise ReportError('line %d: duplicate undecided line for %s' % (n, addr))
        out[addr] = (m.group(2).lower(), m.group(3).strip())
    return out


def parse_candidates(text):
    """Parse the '## Candidates' section of a probe report into {addr: (side,
    reason)} (Task 28): `candidate <0xADDR> fork <reason>` for a row whose fork
    body the worker adopted, `candidate <0xADDR> neither <reason>` for one it
    did not build.  Same tolerance as parse_undecided (address-free prose is
    skipped, a mangled line fails); a missing section is {}; a duplicate line
    is refused."""
    out = {}
    in_section = False
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if line.startswith('#'):
            in_section = line.lstrip('#').strip().lower() == 'candidates'
            continue
        if not in_section or not line or re.fullmatch(r'```[^`]*', line):
            continue
        cand = MARKER_RE.sub('', line, count=1).strip().strip('`').strip()
        m = CANDIDATE_RE.match(cand)
        if not m:
            if not re.search(r'0[xX][0-9a-fA-F]+', cand) and \
                    not cand.lower().startswith('candidate'):
                continue
            raise ReportError('line %d: malformed candidate line: %r' % (n, raw))
        addr = '0x%X' % int(m.group(1), 16)
        if addr in out:
            raise ReportError('line %d: duplicate candidate line for %s' % (n, addr))
        out[addr] = (m.group(2).lower(), m.group(3).strip())
    return out


def parse_kept(text):
    """Parse the '## Kept' section of an import report into {addr: reason}
    (Task 19): base-body rows whose body the worker found already matches
    the original, so it neither imported nor parked them.  Same tolerance as
    parse_undecided; a missing section is {}.  A row cannot be both kept and
    imported (a `## Imported` line), and a duplicate kept line is refused."""
    out, imported = {}, set()
    section = None
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if line.startswith('#'):
            section = line.lstrip('#').strip().lower()
            continue
        if not line or re.fullmatch(r'```[^`]*', line):
            continue
        cand = MARKER_RE.sub('', line, count=1).strip().strip('`').strip()
        if section == 'imported':
            m = IMPORTED_RE.match(cand)
            if m:
                imported.add('0x%X' % int(m.group(1), 16))
            continue
        if section != 'kept':
            continue
        m = KEPT_RE.match(cand)
        if not m:
            # The same tolerance as parse_report and parse_undecided: prose
            # that names no address and does not try to be a kept line
            # ("None.", "every row was parked this round") is skipped; a
            # mangled line still fails.
            if not re.search(r'0[xX][0-9a-fA-F]+', cand) and \
                    not cand.lower().startswith('kept'):
                continue
            raise ReportError('line %d: malformed kept line: %r' % (n, raw))
        addr = '0x%X' % int(m.group(1), 16)
        if addr in out:
            raise ReportError('line %d: duplicate kept line for %s' % (n, addr))
        out[addr] = m.group(2).strip()
    both = sorted(set(out) & imported)
    if both:
        raise ReportError('%s listed under both ## Imported and ## Kept; a row is one or the '
                          'other' % ', '.join(both))
    return out


# The history rule's way to undo one function (first's 021a6cb0): the
# function is edited back, never the whole file put back, which would
# discard the other rows' changes in a shared file.  %s is the merge base.
EDIT_BACK = ("edit that function back to the merge base's body (`git show %s:<path>` "
             'shows it)')
SHARED_FILE = "do not replace a file that holds other rows' changes"


# The needs of a kept row the Frida phase could not measure (Task 19): parked
# so a later scenario can confirm it.  Only a kept or base-body row parks for
# lack of coverage: a stub or only-theirs row (ours never had the body)
# imports unmeasured instead (rule 1 of the 2026-10-26 ruling,
# _write_unmeasured_evidence).
KEPT_UNMEASURED = 'not exercised; ours reading matches the decompile'
KEPT_EDITED_NEEDS = 'kept row edited then put back; not measured'
OFF_KEPT = ', and take the row off `## Kept`'


def kept_needs(scenarios, detail=None):
    """The `needs` of a kept row nothing measured: the not-exercised wording
    when there is no detail, else '<scenarios>: not measured (<detail>); ours
    reading matches the decompile'."""
    if not detail:
        return KEPT_UNMEASURED
    if not isinstance(scenarios, str):
        scenarios = ', '.join(scenarios)
    return '%s: not measured (%s); ours reading matches the decompile' % (scenarios, detail)


ONLY_THEIRS_REMOVE = ("remove the function you added (and its declaration) so the file is back to "
                      "the merge base's text for it (`git show %s:<path>` shows it)")


def park_text(address, mb, needs, edit_back=True, kept=False, only_theirs=False):
    """How a worker parks one import row (the import brief's rule): the
    function edited back, its evidence deleted, its own file under
    docs/wso/import-parked/ (one per row, so parallel batches never
    conflict), one commit.  A kept row's body was never changed
    (`edit_back` False): nothing to edit back, two changes in the commit.  A
    `kept` row also leaves the report's ## Kept section (a row both parked and
    kept is a check problem)."""
    stem = address[2:].upper() if address.lower().startswith('0x') else address.upper()
    # needs as a JSON string literal: a diff detail carries quotes of its own.
    if not edit_back:
        return ('the body was never changed, so edit nothing back: delete docs/evidence/%s.md, '
                'write docs/wso/import-parked/%s.json {"address", "name", "file", "batch", '
                '"needs"} with needs %s, and commit the two together%s'
                % (stem, stem, json.dumps(needs), OFF_KEPT if kept else ''))
    back = ONLY_THEIRS_REMOVE if only_theirs else EDIT_BACK
    return ('%s, delete docs/evidence/%s.md, write docs/wso/import-parked/%s.json '
            '{"address", "name", "file", "batch", "needs"} with needs %s, and commit '
            'the three together%s; %s' % (back % mb, stem, stem, json.dumps(needs),
                                          OFF_KEPT if kept else '', SHARED_FILE))


def unmeasured_needs(run, left=()):
    """The `needs` of an import row no scenario that ran exercised: the
    scenarios that ran, and the ones coverage left out (not hit per coverage)."""
    text = ', '.join(run) + ': not exercised'
    if left:
        text += '; not hit per coverage: ' + ', '.join(left)
    return text


COVERAGE_NOT_HIT = 'coverage: no scenario hits it'
COVERAGE_UNHOOKABLE = 'Frida refuses the hook; coverage'


def unmeasured(address, run, park):
    return '%s: exercised by no scenario (%s); park it: %s' % (address, ', '.join(run), park)


def _merge_base(worktree, base):
    """Where the worktree's HEAD left base (`git merge-base base HEAD`), as
    a short sha; base itself when git cannot tell."""
    r = subprocess.run(['git', '-C', worktree, 'merge-base', base, 'HEAD'],
                       capture_output=True, text=True)
    sha = r.stdout.strip() if r.returncode == 0 else ''
    return sha[:12] if sha else base


X86_MAP_REL = 'build/windows-msvc-x86/RelWithDebInfo/iwd2r.map'


FRIDA_BUILD_TARGETS = 'iwd2r iwd2r_injector iwd2r_recorder'


def frida_build(worktree, log_path=None):
    """Our x86 build of a batch worktree, for its Frida phase (spec B): the
    worktree synced into its own Windows checkout (play.win_checkout:
    %USERPROFILE%\\iwd2r-build-<worktree folder name>, never the driver's,
    whatever IWD2R_WIN_CHECKOUT says) and built there with the x86 preset,
    the compiler output in log_path: our exe and the oracle's injector and
    recorder (FRIDA_BUILD_TARGETS), which the original side runs from the
    same checkout (probe.py --build-checkout), so the recorder writing the
    original's state dumps and the dumper writing ours come from one tree and
    the dump guard compares like with like (the driver checkout's recorder,
    older than schema.json, failed every b014/i001 guard).  Returns
    (checkout, map path): the map beside the exe names the RVAs of the
    bodies under test.  Raises on any failure (play exits with
    SystemExit)."""
    play = _tool('play')
    oracle = _tool('oracle')
    checkout = play.win_checkout(worktree, {})
    play.sync_windows(root=worktree, checkout=checkout)
    exe = play.build_windows_x86(checkout=checkout, log_path=log_path,
                                 target=FRIDA_BUILD_TARGETS)
    map_path = os.path.splitext(exe)[0] + '.map'
    if not os.path.isfile(map_path):
        raise RuntimeError('the x86 build left no %s' % map_path)
    for name in (oracle.INJECTOR, oracle.RECORDER):
        path = os.path.join(checkout, oracle.REPLAY_DIR_REL, name)
        if not os.path.isfile(path):
            raise RuntimeError('the x86 build left no %s' % path)
    return checkout, map_path


def _map_checkout(map_path):
    """The Windows checkout whose x86 build wrote map_path (the --frida-map
    override measures the exe beside it), or None when the map is not where
    that build puts it."""
    norm = os.path.normpath(os.path.abspath(map_path))
    rel = os.path.normpath(X86_MAP_REL)
    return norm[:-len(rel) - 1] if norm.endswith(os.sep + rel) else None


def _infrastructure_failure(traces):
    """truth.infrastructure_failure: why the Frida run's traces are the
    batch's exception (a missing loaded RVA, or every scenario failed with no
    status or alike on both sides), else ''."""
    return _tool('truth').infrastructure_failure(traces)


def _schema_failure(traces):
    """The scenarios whose state dumps truth.compare_dumps refused on the
    replay schema (a recorder or our dumper out of step with schema.json:
    every scenario's dump guard would fail on it, whatever the rows did),
    with its detail, else ''."""
    return '; '.join('%s: %s' % (sc, t['dumps'].get('detail'))
                     for sc, t in sorted(traces.items())
                     if isinstance(t, dict) and isinstance(t.get('dumps'), dict)
                     and t['dumps'].get('invalid'))


def _write_trace_evidence(path, probe, orig_path, ours_path, keep_exemption=False):
    """Rewrite one evidence file's front matter to `kind: trace` with the
    `trace_source:` of a measured Frida pair (truth.trace_source_line) and no
    `exemption:` (kept with keep_exemption: a wave-1 row's documented
    deviation; check_evidence ignores it beside kind: trace); every other
    line, and the line endings, stay as they are."""
    truth = _tool('truth')
    with open(path, encoding='utf-8', newline='') as f:
        lines = f.read().splitlines(keepends=True)
    if not lines or lines[0].strip() != '---':
        raise ValueError('%s has no front matter' % path)
    end = next((i for i in range(1, len(lines)) if lines[i].strip() == '---'), None)
    if end is None:
        raise ValueError('%s has an unclosed front matter' % path)
    eol = '\r\n' if lines[0].endswith('\r\n') else '\n'
    source = truth.trace_source_line(probe, orig_path, ours_path)
    head, kind_at, source_at = [], None, None
    for line in lines[1:end]:
        name = line.partition(':')[0].strip()
        if name == 'exemption' and not keep_exemption:
            continue
        if name == 'kind' and kind_at is None:
            kind_at = len(head)
            line = 'kind: trace' + (line[len(line.rstrip('\r\n')):] or eol)
        elif name == 'trace_source' and source_at is None:
            source_at = len(head)
            line = 'trace_source: %s' % source + (line[len(line.rstrip('\r\n')):] or eol)
        head.append(line)
    if kind_at is None:
        kind_at = len(head)
        head.append('kind: trace' + eol)
        if source_at is not None and source_at >= kind_at:
            source_at += 1
    if source_at is None:
        head.insert(kind_at + 1, 'trace_source: %s%s' % (source, eol))
    with open(path, 'w', encoding='utf-8', newline='') as f:
        f.write(''.join([lines[0]] + head + lines[end:]))


def _write_unmeasured_evidence(path, needs):
    """Rewrite one evidence file's `exemption:` to the unmeasured WSO import
    marking (rule 1 of the 2026-10-26 ruling: where ours never had the body
    the import no longer needs coverage), leaving `kind:` and every other
    line, and the line endings, as they are."""
    wave = _tool('wave')
    with open(path, encoding='utf-8', newline='') as f:
        lines = f.read().splitlines(keepends=True)
    if not lines or lines[0].strip() != '---':
        raise ValueError('%s has no front matter' % path)
    end = next((i for i in range(1, len(lines)) if lines[i].strip() == '---'), None)
    if end is None:
        raise ValueError('%s has an unclosed front matter' % path)
    eol = '\r\n' if lines[0].endswith('\r\n') else '\n'
    marked = 'exemption: %s: %s' % (wave.UNMEASURED_WSO, needs)
    head, hit = [], False
    for line in lines[1:end]:
        if not hit and line.partition(':')[0].strip() == 'exemption':
            hit = True
            line = marked + (line[len(line.rstrip('\r\n')):] or eol)
        head.append(line)
    if not hit:
        kind_at = next((i for i, l in enumerate(head)
                        if l.partition(':')[0].strip() == 'kind'), None)
        head.insert(0 if kind_at is None else kind_at + 1, marked + eol)
    with open(path, 'w', encoding='utf-8', newline='') as f:
        f.write(''.join([lines[0]] + head + lines[end:]))


def _undo_trace_evidence(path, saved_text, batch_id=None):
    """Undo what _write_trace_evidence changed on the CURRENT file: `kind:` back
    to the saved copy's value, the `trace_source:` line dropped, and the saved
    `exemption:` line put back when the file lacks one; every other line (prose
    edited since) stays.  `kind:` is reset only while the file still reads
    `kind: trace` (a worker who replaced it with oracle or replay evidence is
    left as written), `trace_source:` dropped only when it is this batch's
    frida line (`frida <batch_id>-...`; any frida line without batch_id), and
    `exemption:` put back only when `kind:` went back to read-only.  True when done that way; False when the current front
    matter cannot be read, in which case the saved copy replaces the file."""
    with open(path, encoding='utf-8', newline='') as f:
        lines = f.read().splitlines(keepends=True)
    saved = saved_text.splitlines(keepends=True)

    def front(ls):
        if not ls or ls[0].strip() != '---':
            return None
        end = next((i for i in range(1, len(ls)) if ls[i].strip() == '---'), None)
        return None if end is None else end
    end, send = front(lines), front(saved)
    field = lambda ls, e, n: next((l for l in ls[1:e] if l.partition(':')[0].strip() == n), None)
    old_kind = field(saved, send, 'kind') if send is not None else None
    if end is None or old_kind is None:
        with open(path, 'w', encoding='utf-8', newline='') as f:
            f.write(saved_text)
        return False
    eol = '\r\n' if lines[0].endswith('\r\n') else '\n'
    old_ex = field(saved, send, 'exemption')
    head, have_ex, reset = [], False, False
    prefix = 'frida %s-' % batch_id if batch_id else 'frida '
    for line in lines[1:end]:
        name, _, value = line.partition(':')
        name, value = name.strip(), value.strip()
        if name == 'trace_source' and value.startswith(prefix):
            continue
        if name == 'exemption':
            have_ex = True
        if name == 'kind' and value == 'trace' and not reset:
            reset = True
            line = old_kind.rstrip('\r\n') + (line[len(line.rstrip('\r\n')):] or eol)
        head.append(line)
    if (reset and old_ex is not None and not have_ex
            and old_kind.partition(':')[2].strip() == 'read-only'):
        at = next((i for i, l in enumerate(head) if l.partition(':')[0].strip() == 'kind'), len(head) - 1)
        head.insert(at + 1, old_ex.rstrip('\r\n') + eol)
    with open(path, 'w', encoding='utf-8', newline='') as f:
        f.write(''.join([lines[0]] + head + lines[end:]))
    return True


def mechanical_checks(batch, worktree, verdicts, run=subprocess.run,
                      kind='verdicts', transcript=None, covered_files=(),
                      probe=(), base='first', undecided=(), frida_lines=None,
                      kept=(), tests=True, candidates=None):
    """Problems that stop a batch before any review; [] when clean.

    Each problem says what failed and how to fix it (it is the worker's
    fix-round prompt).  An import batch (kind 'import', ruling R63) has no
    verdict lines or evidence phrases to check; the tool tests,
    check_evidence and parity still run.  With a transcript, the worker's
    forbidden tool calls (transcript_checks) join the problems.  `probe`
    holds the address keys the driver recorded from an oracle probe pass:
    only those (and a `probe-<batch id>-...` ours case) need the
    'corroborated by WSO' phrase (ruling R71).  Uncommitted paths, and
    paths changed against `base` outside ALLOWED_PATHS, are problems.
    `undecided` holds the addresses of the report's ## Undecided section
    (ruling R73): such a row needs no verdict line, and a verdict line for
    it too is a problem.  For an import batch, an evidence file changed
    against the merge base whose trace_source is a frida line the driver did
    not write (`frida_lines`, {address key: line} of the lines its Frida
    phase wrote) is a problem: the driver writes frida evidence.  Also for an
    import batch: a parked row (a docs/wso/import-parked file) whose
    docs/evidence file still exists is a problem (close's own wording), and a
    `kept` row (the report's ## Kept section, address keys) must be a
    base-body row of the batch, unparked, with its body unchanged and an
    evidence file.  `tests` False skips the three test steps at the end (the
    worker's selfcheck runs the rest); the worker's `.report.md` draft is never
    an uncommitted path.  A probe batch (kind 'probes', Task 28) has no verdict
    lines; `candidates` ({addr: (side, reason)} of the report's ## Candidates)
    must account for every row (_probe_candidate_problems)."""
    wave = _tool('wave')
    problems = []
    if transcript:
        problems += transcript_checks(transcript, covered_files)
    key = wave._addr_key
    rows = batch.get('rows', [])
    row_keys = {key(r['address']) for r in rows}
    given = {key(a): a for a in verdicts}
    und = {key(a): a for a in undecided}
    if kind in ('import', 'probes'):
        if und:
            problems.append('Undecided is for wave-1 batches; remove the '
                            '## Undecided section')
        if kind == 'probes' and given:
            problems.append('a probe batch has no verdict lines (the driver decides every '
                            'row); remove them and the ## Verdicts section: %s'
                            % ', '.join(sorted(given.values())))
        rows, given, und = [], {}, {}
    if kind not in ('import', 'probes'):
        for k, a in sorted(given.items()):
            side, case = verdicts[a][0], verdicts[a][1]
            if side in ('fork', 'neither') and \
                    wave.deciding_kind(case) == 'dump reading':
                problems.append(
                    '%s: a %s verdict decided by a dump reading (%s); a fork '
                    'or neither verdict needs a probe, replay or trace case '
                    '(probe-N, a replay checkpoint or a trace id); dump-* and '
                    'readout-* decide only ours' % (a, side, case))
    for r in rows:
        if r.get('class') == 'ambiguous' or (
                'class' not in r and not r.get('verdict')):
            if key(r['address']) not in given and \
                    key(r['address']) not in und:
                problems.append('%s: no verdict line in the report; add '
                                '"verdict <addr> ours|fork|neither case <id>" '
                                'under ## Verdicts (or, if no probe, replay or '
                                'trace case you can run decides a fork or '
                                'neither reading, list it under ## '
                                'Undecided)' % r['address'])
    for k, a in sorted(und.items()):
        if k not in row_keys:
            problems.append('%s: undecided line for an address outside this '
                            'batch; remove that line' % a)
        elif k in given:
            problems.append('%s: both a verdict line and an undecided line; '
                            'keep one (a row you cannot decide gets no '
                            'verdict line, never ours)' % a)
    inside = {k: a for k, a in und.items() if k in row_keys and k not in given}
    if inside:
        # An undecided row leaves the batch with our body: a changed body
        # would reach the base as an import without a probe.
        try:
            changed = {key('0x%X' % addr) for _p, addr, _n in
                       _tool('check_evidence').touched(worktree, base)}
        except Exception as e:
            changed = set()
            problems.append('cannot tell whether the undecided rows kept '
                            'our bodies (%r); fix it and rerun' % (e,))
        mb = _merge_base(worktree, base)
        for k, a in sorted(inside.items()):
            if k in changed:
                problems.append('%s: listed under ## Undecided but its body '
                                'changed against %s; %s and commit; %s.'
                                % (a, base, EDIT_BACK % mb, SHARED_FILE))
    for k, a in given.items():
        if k not in row_keys:
            problems.append('%s: verdict for an address outside this batch; '
                            'remove that line' % a)
    probe_keys = {key(a) for a in probe}
    for r in rows:
        k = key(r['address'])
        if k not in given:
            continue
        rel = '%s/%s.md' % (wave.EVIDENCE_DIR, k.upper())
        text, err = wave._read_evidence(worktree, batch['branch'], r['address'])
        if text is None:
            problems.append('%s: missing; write %s (caps, no 0x) and commit it' % (rel, rel))
            continue
        side, case = verdicts[given[k]][0], str(verdicts[given[k]][1])
        if side == 'fork' and not wave.KIND_RE.search(text):
            problems.append("%s: no 'kind:' line in the front matter; add "
                            "'kind: oracle' or 'kind: replay'" % rel)
        driver_probe = k in probe_keys or (
            side == 'ours' and batch.get('id') is not None
            and case.startswith('probe-%s-' % batch['id']))
        paras = wave.evidence_paragraphs(text)
        probe_msg = driver_probe and not any(
            wave.EVIDENCE_PHRASES[1] in para for para in paras)
        if probe_msg:
            # A probe pass says ours matches the original (ruling R57).
            problems.append("%s: its verdict case is a probe; write the "
                            "sentence 'corroborated by WSO at <sha>: ...; "
                            "case %s decides' in the ## WSO delta section"
                            % (rel, case))
        if not probe_msg and not wave._evidence_decided_by(text, case):
            problems.append(
                '%s: docs/evidence/%s.md has no sentence reading "disagreed '
                'with WSO at <sha>: ...; case %s decides" (or "corroborated '
                'by WSO ..."); write it in the ## WSO delta section'
                % (r['address'], k.upper(), case))
        if batch.get('fork_base') and not wave._cites_pin(
                text, batch['fork_base']):
            problems.append(
                '%s: no "disagreed with WSO at <sha>" / "corroborated by WSO '
                'at <sha>" line names this batch\'s pin %s'
                % (rel, batch['fork_base'][:7]))
    dirty, bad = _git_paths(run, worktree, ('status', '--porcelain', '-uall'),
                            lambda l: l[3:].split(' -> ')[-1].strip('"'))
    if bad:
        problems.append(bad)
    dirty = [p for p in dirty if p != REPORT_DRAFT]
    if dirty:
        problems.append('uncommitted paths in the worktree; commit or '
                        'delete: %s' % ', '.join(sorted(dirty)))
    # The base is where the branch left `base`, not `base` itself: commits
    # that landed on first meanwhile (merged into the batch before the close)
    # are not the worker's edits.
    mbs, bad = _git_paths(run, worktree,
                          ('merge-base', base, batch.get('branch') or 'HEAD'),
                          str.strip)
    if bad:
        problems.append(bad)
    changed, bad = _git_paths(run, worktree,
                              ('diff', '--name-only') + (
                                  (mbs[0], 'HEAD') if mbs
                                  else ('%s...HEAD' % base,)),
                              str.strip)
    if bad:
        problems.append(bad)
    problems += _foreign_frida_lines(worktree, changed, frida_lines or {},
                                     None if kind == 'import' or not mbs else mbs[0])
    if kind == 'import':
        problems += _kept_and_parked_problems(batch, worktree, base, kept, key, changed)
    if kind == 'probes':
        problems += _probe_candidate_problems(batch, worktree, base, candidates, key)
    outside = sorted(p for p in changed if not _path_allowed(p))
    if outside:
        # These files hold no batch row, so the whole file goes back, from
        # the merge base the diff above was taken against.
        held = mbs[0][:12] if mbs else base
        problems.append('changed outside the paths a batch may touch (%s): '
                        '%s; put back the merge base\'s copy of each (`git show '
                        '%s:<path> > <path>`, or `git rm <path>` when the merge base '
                        'lacks the file; no batch row lives there) and commit'
                        % (', '.join(ALLOWED_PATHS), ', '.join(outside), held))
    steps = [] if not tests else [
        (['python3', '-I', 'tests/test_tools.py'], 'tests/test_tools.py'),
        (['python3', '-I', 'tools/check_evidence.py', '--base', 'first'],
         'tools/check_evidence.py'),
    ]
    if tests and os.path.isfile(_vanilla_exe()):
        steps.append((_parity_python() + ['tools/parity/run.py', '--base', 'first'],
                      'tools/parity/run.py'))
    # docs/MAP.md is not the batch's: the driver regenerates it once at close,
    # on the merged tree (_regen_map), so parallel batches never conflict.
    for argv, label in steps:
        proc = run(argv, cwd=worktree, capture_output=True, text=True)
        if proc.returncode != 0:
            problems.append('%s failed (exit %s); last lines: %s; fix it and '
                            'rerun' % (label, proc.returncode, _tail(proc)))
    return problems


def kept_edited_rows(batch, worktree, key):
    """{address key: address} of the rows the phase recorded `kept` (measured
    on our body) whose body on the branch is no longer the measured one: they
    get one message, from the section check (Task 30), nothing else."""
    ce = _tool('check_evidence')
    return {key(r['address']): r['address'] for r in batch.get('rows', [])
            if r.get('outcome') == 'kept' and r.get('measured_body')
            and ce.body_digest(worktree, r['address'], r.get('file')) != r['measured_body']}


def _measured_body_problems(batch, worktree, parked, key, moved=None):
    """A row the Frida phase wrote trace evidence for (`measured_body` in the
    batch state: the digest of its body then) whose body on the branch is now
    another one: its trace measured a body that is not the one that lands.
    A dependency (`measured_deps`) is held to it only while the batch still
    changes it against the merge base (`moved`, address keys; None: every
    one), as close does: edited back to the merge base's body it is no longer
    the batch's."""
    ce = _tool('check_evidence')
    out = []
    edited_kept = kept_edited_rows(batch, worktree, key)
    for r in batch.get('rows', []):
        want = r.get('measured_body')
        if want and key(r['address']) not in parked and key(r['address']) not in edited_kept and \
                ce.body_digest(worktree, r['address'], r.get('file')) != want:
            out.append('%s: body changed after the Frida phase; park it or edit it back to the '
                       'measured body' % r['address'])
    for k, want in sorted((batch.get('measured_deps') or {}).items()):
        if moved is not None and k not in moved:
            continue
        if k not in parked and ce.body_digest(worktree, '0x' + k) != want:
            out.append('0x%s: body changed after the Frida phase; park it or edit it back to '
                       'the measured body' % k.upper())
    return out


def _kept_and_parked_problems(batch, worktree, base, kept, key, changed_paths=()):
    """The import checks on parked and kept rows (Task 19).  A parked row's
    evidence file must be gone: close refuses it, so the worker's own fix
    round removes it; that covers the batch's own rows and the parked files
    this branch added or changed against the merge base (`changed_paths`): a
    parked file `first` already carried, for a row outside the batch, is not
    this batch's problem.  A kept row must be a base-body row of the batch,
    or an only-theirs row ours already carries a body for (our src labels the
    address at the merge base), not parked, with the merge base's body and an
    evidence file."""
    wave = _tool('wave')
    problems = []
    parked, _bad = wave._parked_rows(worktree)
    rows = {key(r['address']): r for r in batch.get('rows', [])}
    changed_keys = {wave.parked_path_key(p) for p in changed_paths} - {None}
    for k in sorted(parked):
        if k not in rows and k not in changed_keys:
            continue
        rel = wave._evidence_rel(k)
        if os.path.isfile(os.path.join(worktree, rel)):
            addr = rows[k]['address'] if k in rows else '0x' + k.upper()
            problems.append('%s: parked (%s) but %s still exists; delete it'
                            % (addr, wave.parked_rel(k), rel))
    moved = None
    if kept or batch.get('measured_deps'):
        try:
            moved = {key('0x%X' % a) for _p, a, _n in
                     _tool('check_evidence').touched(worktree, base)}
        except Exception as e:
            what = ('whether the kept rows kept our bodies' if kept
                    else 'which dependencies the batch still changes')
            problems.append('cannot tell %s (%r); fix it and rerun' % (what, e))
    problems += _measured_body_problems(batch, worktree, parked, key, moved)
    if not kept:
        return problems
    moved = moved or set()
    mb = _merge_base(worktree, base)
    for a in sorted(kept, key=key):
        k = key(a)
        row = rows.get(k)
        # Task 40: an only-theirs row ours already carries a body for (our
        # src labels the address at the merge base) is a present row: it
        # reports under ## Kept like a base-body row, so the only-theirs
        # refusal below fires only for a row without a body there.
        present = (row is not None and row.get('category') == 'only_theirs'
                   and wave._labelled_at(worktree, mb, row.get('address')))
        if row is None:
            problems.append('%s: kept line for an address outside this batch; remove that '
                            'line' % a)
        elif row.get('category') == 'only_theirs' and not present:
            problems.append('%s: listed under ## Kept but ours has no body there; an only-theirs '
                            'row is never kept: import it or park it (remove the function)' % a)
        elif row.get('category') != 'theirs_changed_ours_base' and not present:
            problems.append('%s: listed under ## Kept but ours is a stub there; a stub is '
                            'never kept: import it or park it' % a)
        elif k in parked:
            problems.append('%s: both kept and parked (%s); keep one (delete the parked '
                            'file, or take the row off ## Kept)' % (a, wave.parked_rel(k)))
        else:
            if k in moved and k not in kept_edited_rows(batch, worktree, key):
                problems.append('%s: listed under ## Kept but its body changed against %s; keep '
                                'the change and move the row to ## Imported (take it off '
                                '## Kept), or edit it back and park it (`needs`: `%s`): %s'
                                % (a, base, KEPT_EDITED_NEEDS, park_text(
                                    a, mb, KEPT_EDITED_NEEDS, kept=True)))
            if not os.path.isfile(os.path.join(worktree, wave._evidence_rel(k))):
                problems.append('%s: listed under ## Kept but %s does not exist; write %s (caps, '
                                'no 0x) (kind read-only, an exemption: giving your reading of '
                                'why ours matches; no verdict phrase, no frida line) and '
                                'commit it' % (a, wave._evidence_rel(k), wave._evidence_rel(k)))
    return problems


def _mb_evidence_kind(worktree, base, rel):
    """The `kind:` of an evidence file at the merge base of the worktree's HEAD
    and `base`, or None (no file, no kind)."""
    r = subprocess.run(['git', '-C', worktree, 'show', '%s:%s' % (_merge_base(worktree, base), rel)],
                       capture_output=True, text=True)
    m = _tool('wave').KIND_RE.search(r.stdout) if r.returncode == 0 else None
    return m.group(1) if m else None


def _probe_candidate_problems(batch, worktree, base, candidates, key):
    """The probe batch's checks (Task 28), in the worker's terms.  Every row
    needs one `candidate` line (none for an address outside the batch).  Before
    the Frida phase a `fork` candidate has its body changed against the merge
    base (adopted) and an evidence file that does not carry a frida case, a
    `neither` candidate its body untouched.  After it (the row's recorded
    `outcome`): resolved keeps the body the phase measured; kept or found must
    be back on our body, evidence file included.  No function outside the
    batch's rows may change."""
    wave = _tool('wave')
    ce = _tool('check_evidence')
    problems = []
    cand = {key(a): (a, sc) for a, sc in (candidates or {}).items()}
    rows = {key(r['address']): r for r in batch.get('rows', [])}
    for k, (a, _sc) in sorted(cand.items()):
        if k not in rows:
            problems.append('%s: candidate line for an address outside this batch; remove that '
                            'line' % a)
    try:
        touched = {key('0x%X' % addr): name for _p, addr, name in ce.touched(worktree, base)}
    except Exception as e:
        return problems + ['cannot tell which bodies the batch changed (%r); fix it and rerun' % (e,)]
    mb = _merge_base(worktree, base)
    for k, r in sorted(rows.items()):
        a = r['address']
        rel = '%s/%s.md' % (wave.EVIDENCE_DIR, k.upper())
        path = os.path.join(worktree, rel)
        text = None
        if os.path.isfile(path):
            with open(path, encoding='utf-8') as f:
                text = f.read()
        if k not in cand:
            problems.append('%s: not accounted for in the report; list it under ## Candidates '
                            '(`candidate %s fork <reason>` once you adopted the fork body, '
                            '`candidate %s neither <reason>` for a row you did not build)'
                            % (a, a, a))
            continue
        side = cand[k][1][0]
        outcome = r.get('outcome')
        if outcome in ('resolved', 'fork-equal'):
            want = r.get('measured_body')
            if want and ce.body_digest(worktree, a, r.get('file')) != want:
                problems.append('%s: body changed after the Frida phase; edit it back to the '
                                'body the "frida: trace evidence" commit measured' % a)
            continue
        if outcome in ('kept', 'found') or side == 'neither':
            if k in touched:
                problems.append('%s: stays on the probe list with our body but its function is '
                                'changed against %s; %s and commit; %s.'
                                % (a, base, EDIT_BACK % mb, SHARED_FILE))
            if outcome in ('kept', 'found'):
                was = subprocess.run(['git', '-C', worktree, 'show', '%s:%s' % (mb, rel)],
                                     capture_output=True, text=True)
                if was.returncode == 0 and text != was.stdout:
                    problems.append('%s: stays on the probe list; put %s back as the merge base '
                                    'has it (`git show %s:%s` shows it) and commit'
                                    % (a, rel, mb, rel))
                elif was.returncode != 0 and text is not None:
                    problems.append('%s: stays on the probe list and the merge base has no %s; '
                                    'delete it and commit' % (a, rel))
            continue
        if r.get('reading') == 'neither':
            problems.append('%s: its reading is `neither`, which adopting a body cannot test; '
                            'list it as `candidate %s neither <reason>` and leave its body ours'
                            % (a, a))
            continue
        kind = wave.KIND_RE.search(text) if text is not None else None
        have = kind.group(1) if kind else None
        was = _mb_evidence_kind(worktree, base, rel)
        bad = next((x for x in (have, was) if x not in (None, 'read-only')), None)
        if bad:
            problems.append('%s: evidence is kind %s (%s); a fork candidate needs kind: '
                            'read-only both on the branch and at the merge base (the driver '
                            'rewrites it to trace). List the row as `candidate %s neither '
                            'evidence is kind %s; needs a hand ruling`, edit its body back and '
                            'put %s back as the merge base has it'
                            % (rel, bad, 'on the branch' if bad == have else 'at the merge base',
                               a, bad, rel))
            continue
        if k not in touched:
            problems.append('%s: listed as `candidate fork` but its body is unchanged against %s; '
                            'adopt the fork body, or list the row as `candidate %s neither '
                            '<reason>`' % (a, base, a))
        if text is None:
            problems.append('%s: missing; write %s (caps, no 0x) and commit it' % (rel, rel))
        elif re.search(r'case\s+frida-%s-' % re.escape(batch.get('id') or 'p'), text):
            problems.append('%s: carries a `case frida-...` sentence; the driver writes the '
                            'deciding case when the Frida phase decides, remove it' % rel)
        elif not wave.KIND_RE.search(text):
            problems.append("%s: no 'kind:' line in the front matter; add 'kind: read-only'" % rel)
    for k, name in sorted(touched.items()):
        if k not in rows:
            problems.append('0x%s (%s): changed against %s but not a row of this batch; a probe '
                            "batch changes only its rows' functions; %s and commit"
                            % (k.upper(), name, base, EDIT_BACK % mb))
    return problems


def _foreign_frida_lines(worktree, changed, frida_lines, merge_base=None):
    """Problems for the evidence files among `changed` whose trace_source is
    a frida line the driver's phase did not write (frida_lines); with
    `merge_base`, a line the file already had there is inherited, not the
    worker's."""
    ce = _tool('check_evidence')
    out = []
    for rel in sorted(changed):
        m = re.fullmatch(r'docs/evidence/([0-9A-Fa-f]+)\.md', rel)
        if not m:
            continue
        try:
            with open(os.path.join(worktree, rel), encoding='utf-8') as f:
                source = ce.front_matter(f.read()).get('trace_source') or ''
        except OSError:
            continue
        if merge_base and source:
            at = subprocess.run(['git', '-C', worktree, 'show', '%s:%s' % (merge_base, rel)],
                                capture_output=True, text=True)
            if at.returncode == 0 and (ce.front_matter(at.stdout).get('trace_source') or '') == source:
                continue
        if ce.TRACE_SOURCES[1].fullmatch(source) and \
                frida_lines.get(m.group(1).lower()) != source:
            out.append('%s: a frida trace_source the driver did not write; the driver writes '
                       'frida evidence: remove the line (write kind: read-only with an '
                       'exemption: describing your reading; the Frida phase measures the row)'
                       % rel)
    return out


def review_needed(batch, verdicts, seed, sample=0.2):
    """(needed, reason): fork/neither rows always, else a seeded sample."""
    if any(side != 'ours' for side, _case in verdicts.values()):
        return True, 'fork/neither rows'
    batch_id = batch if isinstance(batch, str) else batch.get('id')
    if random.Random('%s:%s' % (seed, batch_id)).random() < sample:
        return True, 'sample'
    return False, ''


# -- worker pools (Task 15) -----------------------------------------------

# reviewer_model is resolved by parse_pool; None only on a hand-built Pool.
Pool = collections.namedtuple(
    'Pool', 'name wrapper model workers reviewer_model', defaults=(None,))
DEFAULT_POOL = 'glm=claude-glm:glm-5.3-flash[1m]:2'
# The short form NAME[:WORKERS] of the two known pool names, per queue.
POOL_DEFAULTS = {
    'glm': {'wave1': ('claude-glm', 'glm-5.3-flash[1m]'),
            'import': ('claude-glm', 'glm-5.3-flash[1m]'),
            'probes': ('claude-glm', 'glm-5.3-flash[1m]')},
    'claude': {'wave1': ('claude', 'sonnet'), 'import': ('claude', 'haiku'),
               'probes': ('claude', 'sonnet')},
}
# A pool's reviewer model unless --reviewer-model says otherwise; any
# other pool reviews with its own worker model.
REVIEWER_DEFAULTS = {'claude': 'sonnet', 'glm': 'glm-5.3-flash[1m]'}
POOL_NAME_RE = re.compile(r'[A-Za-z0-9_-]+')


def _workers_count(text, spec):
    try:
        n = int(text)
    except ValueError:
        n = 0
    if n < 1:
        raise ValueError('pool %r: WORKERS must be a whole number of at '
                         'least 1, not %r' % (spec, text))
    return n


def parse_pool(spec, queue='wave1', reviewer_model=None):
    """A Pool from `NAME=WRAPPER:MODEL[:WORKERS]` (WORKERS defaults to 1),
    or from the short form `NAME[:WORKERS]` of a known name (POOL_DEFAULTS,
    which depend on the queue).  ValueError when malformed.
    reviewer_model overrides REVIEWER_DEFAULTS."""
    if not isinstance(spec, str) or not spec.strip():
        raise ValueError('empty pool spec')
    if '=' in spec:
        name, _, rest = spec.partition('=')
        parts = rest.split(':')
        if len(parts) not in (2, 3) or not parts[0] or not parts[1]:
            raise ValueError(
                'pool %r: expected NAME=WRAPPER:MODEL[:WORKERS]' % spec)
        wrapper, model = parts[0], parts[1]
        workers = _workers_count(parts[2], spec) if len(parts) == 3 else 1
    else:
        name, _, count = spec.partition(':')
        if name not in POOL_DEFAULTS or queue not in POOL_DEFAULTS[name]:
            raise ValueError(
                'pool %r: the short form NAME[:WORKERS] needs a known name '
                '(%s) and a known queue; give NAME=WRAPPER:MODEL[:WORKERS]'
                % (spec, ', '.join(sorted(POOL_DEFAULTS))))
        wrapper, model = POOL_DEFAULTS[name][queue]
        workers = _workers_count(count, spec) if ':' in spec else 1
    if not POOL_NAME_RE.fullmatch(name):
        raise ValueError('pool %r: the name must be letters, digits, - or _'
                         % spec)
    return Pool(name, wrapper, model, workers,
                reviewer_model or REVIEWER_DEFAULTS.get(name, model))


def pools_with_reviewers(pools, reviewer_models):
    """The pools with --reviewer-model NAME=MODEL applied; ValueError for
    a name that is no pool."""
    names = {p.name for p in pools}
    for name in reviewer_models:
        if name not in names:
            raise ValueError('--reviewer-model %s=...: no pool named %s '
                             '(pools: %s)' % (name, name, ', '.join(sorted(names))))
    return [p._replace(reviewer_model=reviewer_models[p.name])
            if p.name in reviewer_models else p for p in pools]


def pool_is_glm(pool):
    """True when Z.ai credits price the pool's turns (ruling R40)."""
    return _rates(pool.model) is not None


def parked_map(value):
    """A pool's `parked` as {batch_id: {at, reason}} (ruling R64).  The
    one-slot form `{batch_id, at, reason}` of the first pool driver, and
    None, read as a map too."""
    if not isinstance(value, dict) or not value:
        return {}
    if isinstance(value.get('batch_id'), str):
        return {value['batch_id']: {'at': value.get('at'),
                                    'reason': value.get('reason')}}
    return {k: v for k, v in value.items() if isinstance(v, dict)}


def _set_parked(holder, mapping):
    holder['parked'] = dict(mapping) or None


def _unpark_everywhere(s, batch_id):
    """Remove batch_id from every pool's parked map (and the legacy
    top-level slot)."""
    for holder in [s] + [p for p in s['pools'].values() if isinstance(p, dict)]:
        m = parked_map(holder.get('parked'))
        if batch_id in m:
            m.pop(batch_id)
            _set_parked(holder, m)


# -- the batch loop ------------------------------------------------------

# Relative to the repo root (ruling R29): one ledger per plan, and the
# driver folder is the status file's directory.
LEDGER_REL = os.path.join('.superpowers', 'sdd', '2026-10-17-wso-integration',
                          'progress.md')
DRIVER_REL = os.path.join('.superpowers', 'sdd', '2026-10-17-wso-integration',
                          'driver')
STATUS_REL = os.path.join(DRIVER_REL, 'status.json')


def status_rel(queue='wave1'):
    """The status file of a queue, relative to the repo root (ruling
    R42): status.json for wave1, status-import.json for import."""
    return os.path.join(DRIVER_REL, _tool('wave').QUEUES[queue]['status'])


DEFAULT_SEED = '2026-10-22'
# The next batch's credits before this run has measured one (ruling R38;
# the plan's ledger example is 1180).
DEFAULT_ESTIMATE = 1200.0
WIP_MESSAGE = 'WIP: parked by driver'
# The breaker (ruling R45): this many batch exceptions in a row whose
# worker never produced a transcript halt the run.
BREAKER_LIMIT = 3
STATE_COMMIT_TRAILER = 'Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>'
PUSH_ARGS = ('push', 'iwd2r', 'first:main', 'first:first')
# Exceptions at these stages are not the batch's own: the run stops.
GLOBAL_STAGES = ('cut', 'push')
BLOCKING_RE = re.compile(r'^(?:[-*]|\d+\.)?\s*[*_`]*BLOCKING[*_`]*:')

FIX_PROMPT = """# Fix round for batch {id}

The driver's {stage} found the problems below in your work on `{branch}`
(this worktree). Fix every one, commit, and write your report again.
{frozen_rule}{report_rule}
{selfcheck_rule}

{problems}

The original brief follows.

---

{brief}"""


# The last rule of both brief templates and of every fix round (Task 24).
SELFCHECK_RULE = ("Before you return: write the report to `.report.md` in the worktree, run "
                  "`python3 -I tools/wave.py selfcheck --batch {id}`, fix every line it prints "
                  "and run it again until it prints `selfcheck: OK`, then return the same text "
                  "as your report.")

# The top of a fix round when the Frida phase measured rows (Task 30): their
# bodies are evidence, and three fix rounds lost an hour each undoing them.
FROZEN_RULE = (
    "**Frozen bodies.** The driver's Frida phase has measured these functions; their "
    "bodies are evidence now. Do not edit them for any reason \u2014 not for a lint, not "
    "for parity, not to \"put them back\". A parity RED on one of them is waived with a "
    "`parity_waiver:` line in its evidence file; a lint WARNING needs no change at all. "
    "A row listed here stays in the report section the driver put it in (`## Kept` for "
    "a base-body row measured on our body, `## Imported` for an import, `candidate "
    "<0xADDR> fork` for a probe row the phase decided).\n{rows}\n\n")


def frozen_rule(frozen):
    """The frozen-bodies block for [(address, name)], '' when there are none."""
    if not frozen:
        return ''
    return FROZEN_RULE.format(rows='\n'.join('- %s %s' % (a, n) for a, n in frozen))


FIX_REPORT_RULES = {
    'verdicts': ("Write the full `## Verdicts` section, one line for every row you "
                 "decide, not only the rows you fixed. `## Verdicts` holds verdict lines "
                 "only; put notes under `## Notes`. A row whose reading is fork or neither, "
                 "with no probe, replay or trace case you can run to decide it, gets no "
                 "verdict line: keep our body and list it under `## Undecided` as "
                 "`undecided <0xADDR> <fork|neither> <one-line reason>`. Never write "
                 "`ours` for such a row."),
    'probes': ("A row the phase decided `fork` keeps its `candidate <0xADDR> fork <reason>` "
               "line; `neither` is only for a row you did not build, or edited back to the "
               "merge base because the phase could not measure it. "
               "This is a probe batch: do not write a `## Verdicts` or `## Undecided` section. "
               "Write the full `## Candidates` section again, one line for every row: "
               "`candidate <0xADDR> fork <one-line reason>` for a row whose fork body you "
               "adopted (and that still stands), `candidate <0xADDR> neither <one-line reason>` "
               "for a row you did not build or whose body you edited back."),
    'import': ("A base-body row the phase measured equal on OUR body stays under `## Kept` "
               "as `kept <0xADDR> <one-line reason>`; `## Imported` lists only the rows whose "
               "body you changed, one `imported <0xADDR> <evidence kind>` line each; a row is "
               "never in both. "
               "This is an import batch: do not write a `## Verdicts` section at all, "
               "not even an empty one or a placeholder line. Report as the brief says: "
               "`## Imported` with one `imported <0xADDR> <evidence kind>` line per "
               "function, then the commits you made, the evidence files you wrote and "
               "anything you could not import (the rows you parked, with their parked "
               "files). A base-body row you keep as ours (its body already matches the "
               "original) is listed under `## Kept` as `kept <0xADDR> <one-line reason>`, "
               "never also under `## Imported`; repeat `## Kept` for the kept rows you did "
               "not park, a parked row leaves it."),
}


def fix_prompt(batch_id, stage, branch, brief, problems, kind='verdicts', frozen=()):
    """The fix-round prompt: the problems, the report rule for the batch's
    kind (a verdicts batch rewrites ## Verdicts; an import batch has none),
    then the original brief.  `frozen` is [(address, name)] of the rows the
    Frida phase measured: their block opens the round."""
    return FIX_PROMPT.format(
        id=batch_id, frozen_rule=frozen_rule(frozen), stage=stage, branch=branch, brief=brief,
        report_rule=FIX_REPORT_RULES[kind if kind in ('import', 'probes') else 'verdicts'],
        selfcheck_rule=SELFCHECK_RULE.format(id=batch_id),
        problems='\n'.join('- ' + p for p in problems))


def _heading_present(text, *names):
    return any(l.strip().startswith('#') and l.strip().lstrip('#').strip().lower() in names
               for l in text.splitlines())


def parse_imported(text):
    """The addresses ('0x' + uppercase hex) of the report's `## Imported`
    lines; prose and anything unparseable is skipped, as parse_kept does."""
    out, section = set(), None
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith('#'):
            section = line.lstrip('#').strip().lower()
        elif section == 'imported' and line:
            m = IMPORTED_RE.match(MARKER_RE.sub('', line, count=1).strip().strip('`').strip())
            if m:
                out.add('0x%X' % int(m.group(1), 16))
    return out


def _spelling_problems(worktree, base, batch, report_addrs):
    """The misspelt evidence file names (`0x78A2F0.md`, lower case) among the
    batch's rows, the addresses the report names and the functions the branch
    touched, as instructions: the worker renames, nothing is renamed here.
    Returns (problems, keys of the rows with a misspelt file)."""
    ce, wave = _tool('check_evidence'), _tool('wave')
    key = wave._addr_key
    addrs = {}
    for a in [r['address'] for r in batch.get('rows', [])] + list(report_addrs) + \
            list(batch.get('measured_deps') or {}):
        addrs.setdefault(key(a), a)
    try:
        for _p, a, _n in ce.touched(worktree, base):
            addrs.setdefault(key('0x%X' % a), '0x%X' % a)
    except Exception:
        pass
    problems, odd_keys = [], set()
    for k in sorted(addrs):
        rel, old = ce.canonical_evidence(worktree, k)
        odd = ce.misspelt_evidence(worktree, k)
        if not odd:
            continue
        odd_keys.add(k)
        if os.path.isfile(os.path.join(worktree, rel)) or len(odd) > 1:
            files = ([rel] if os.path.isfile(os.path.join(worktree, rel)) else []) + odd
            problems.append('two evidence files for 0x%s: %s; keep one, delete the %s'
                            % (k.upper(), ' and '.join(files) if len(files) < 3
                               else ', '.join(files), 'other' if len(files) < 3 else 'others'))
        else:
            problems.append('%s: misspelt evidence file name; rename it to %s (caps, no 0x): '
                            '`git mv %s %s` and commit' % (old, rel, old, rel))
    return problems, odd_keys


def _first_worktree(worktree, run=subprocess.run):
    """The worktree that has `first` checked out (the driver's, where the
    running batch's live state sits), from `git worktree list --porcelain`;
    None when there is none."""
    r = run(['git', '-C', worktree, 'worktree', 'list', '--porcelain'],
            capture_output=True, text=True)
    if r.returncode != 0:
        return None
    path = None
    for line in (r.stdout or '').splitlines():
        if line.startswith('worktree '):
            path = line.split(' ', 1)[1]
        elif path is not None and line == 'branch refs/heads/first':
            return path
    return None


def selfcheck(batch_id, worktree, report, base='first', run=subprocess.run,
              state_path=None):
    """The driver's mechanical checks that need no driver state, for the
    worker to run on its own report and files before it returns (Task 24):
    [] when clean, else one problem per entry in the driver's own wording.
    The batch id gives the kind (b: verdicts, i: import) and the state file.
    The entry is looked for in this worktree's state file, then in the first
    worktree's (the driver keeps the running batch's live state uncommitted
    there), then in `state_path` (--state); nothing is edited."""
    wave = _tool('wave')
    kind = {'i': 'import', 'p': 'probes'}.get(batch_id[:1], 'verdicts')
    state_rel = 'docs/wso/%s-state.json' % {'import': 'import', 'probes': 'probe'}.get(
        kind, 'wave')
    # The driver holds the running batch's live state uncommitted in the
    # worktree that has `first` checked out, so the batch worktree's copy
    # (committed at the cut) has no entry for it: the first of the three
    # places that has the entry wins, and nothing is copied in (Task 33).
    first_wt = _first_worktree(worktree, run)
    places = ['%s (this worktree)' % state_rel]
    places.append(os.path.join(first_wt, state_rel) + ' (the first worktree)'
                  if first_wt else 'the first worktree (no worktree has first checked out)')
    places.append(os.path.abspath(state_path) + ' (--state)'
                  if state_path else '--state (not given)')
    batch = None
    for place in (os.path.join(worktree, state_rel),
                  os.path.join(first_wt, state_rel) if first_wt else None,
                  state_path):
        if not place:
            continue
        try:
            batch = wave.load_state(place)['batches'].get(batch_id)
        except (OSError, ValueError):
            batch = None
        if batch is not None:
            break
    if batch is None:
        return ['batch %s is not in %s; check the --batch id'
                % (batch_id, ', '.join(places))]
    key = wave._addr_key
    problems = []
    try:
        parsed = parse_report(report)
    except ReportError as e:
        return ['report: %s; fix that line under ## Verdicts (one "verdict <0xADDR> '
                '<fork|ours|neither> case <id>" per row; ## Verdicts holds verdict lines '
                'only, put notes under ## Notes)' % e]
    try:
        listed = parse_undecided(report)
    except ReportError as e:
        return ['report: %s; fix that line under ## Undecided (one "undecided <0xADDR> '
                '<fork|neither> <one-line reason>" per row)' % e]
    kept, imported, candidates = {}, set(), {}
    if kind == 'probes':
        try:
            candidates = parse_candidates(report)
        except ReportError as e:
            return ['report: %s; fix that line under ## Candidates (one "candidate <0xADDR> '
                    '<fork|neither> <one-line reason>" per row)' % e]
        if _heading_present(report, 'verdicts', 'undecided'):
            problems.append('report: a probe report has no ## Verdicts or ## Undecided section; '
                            'remove it (the driver decides every row; report the rows under '
                            '## Candidates)')
        parsed, listed = {}, {}
    elif kind == 'import':
        try:
            kept = parse_kept(report)
        except ReportError as e:
            return ['report: %s; fix that line under ## Kept (one "kept <0xADDR> <one-line '
                    'reason>" per row, for a base-body row you leave as ours; never also '
                    'under ## Imported)' % e]
        imported = parse_imported(report)
        if _heading_present(report, 'verdicts'):
            problems.append('report: an import report has no ## Verdicts section; remove it '
                            '(nobody decides fork, ours or neither in an import; report the '
                            'rows under ## Imported, ## Kept or parked)')
        listed = {}
    else:
        if not _heading_present(report, 'verdicts', 'undecided'):
            problems.append('report: no ## Verdicts section (this is not the report); write '
                            'one "verdict <0xADDR> <fork|ours|neither> case <id>" line per row '
                            'under ## Verdicts')
        auto = {key(a) for a, v in batch.get('verdicts', {}).items()
                if _side_case(v)[1].startswith('triage-')}
        parsed = {a: sc for a, sc in parsed.items() if key(a) not in auto}
        listed = {a: sr for a, sr in listed.items() if key(a) not in auto}
    if kind == 'import':
        parked, _bad = wave._parked_rows(worktree)
        have = {key(a) for a in imported} | {key(a) for a in kept} | set(parked)
        for r in batch.get('rows', []):
            if key(r['address']) not in have:
                problems.append('%s: not accounted for in the report; list it under '
                                '## Imported (`imported %s <evidence kind>`), under ## Kept, '
                                'or park it (docs/wso/import-parked/)' % (r['address'], r['address']))
    spelling, odd = _spelling_problems(
        worktree, base, batch,
        list(parsed) + list(listed) + list(kept) + list(imported) + list(candidates))
    # frida lines the driver's own commits wrote (a fix round) are not the worker's
    mb = _merge_base(worktree, base)
    frida = {}
    ce = _tool('check_evidence')
    names = run(['git', '-C', worktree, 'diff', '--name-only', mb, 'HEAD'],
                capture_output=True, text=True)
    for rel in (names.stdout or '').split():
        m = re.fullmatch(r'docs/evidence/([0-9A-Fa-f]+)\.md', rel)
        if not m:
            continue
        try:
            with open(os.path.join(worktree, rel), encoding='utf-8') as f:
                src = ce.front_matter(f.read()).get('trace_source') or ''
        except OSError:
            continue
        # the line is the driver's when a `frida:` commit on the branch introduced
        # it, whoever edited the file after; a worker's own commit under that
        # subject cannot be told apart here (the driver's real check catches it)
        log = run(['git', '-C', worktree, 'log', '--format=%H%x00%s', mb + '..HEAD', '--', rel],
                  capture_output=True, text=True).stdout
        for entry in log.splitlines():
            sha, _, subject = entry.partition('\x00')
            if not subject.startswith('frida:'):
                continue
            at = run(['git', '-C', worktree, 'show', '%s:%s' % (sha, rel)],
                     capture_output=True, text=True)
            if at.returncode == 0 and (ce.front_matter(at.stdout).get('trace_source') or '') == src:
                frida[m.group(1).lower()] = src
                break
    checks = mechanical_checks(
        dict(batch, id=batch_id), worktree, parsed, run=run, kind=kind, base=base,
        undecided=sorted(listed), frida_lines=frida, kept=sorted(kept), tests=False,
        candidates=candidates)
    gone = {'docs/evidence/%s.md: missing;' % k.upper() for k in odd}
    checks = [p for p in checks if not any(p.startswith(g) for g in gone)]
    return problems + spelling + [' '.join(p.split()) for p in checks]


def _exclude_draft(worktree):
    r = subprocess.run(['git', '-C', worktree, 'rev-parse', '--git-path', 'info/exclude'],
                       capture_output=True, text=True)
    if r.returncode != 0 or not r.stdout.strip():
        return
    path = os.path.join(worktree, r.stdout.strip())
    try:
        have = open(path, encoding='utf-8').read() if os.path.isfile(path) else ''
        if REPORT_DRAFT not in have.splitlines():
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'a', encoding='utf-8') as f:
                f.write(('' if not have or have.endswith('\n') else '\n') + REPORT_DRAFT + '\n')
    except OSError:
        pass


def selfcheck_main(argv):
    """`wave.py selfcheck --batch <id> [--report <file>] [--state <file>]`,
    run in the batch worktree: the report from --report (`-` is stdin), else
    `.report.md` there; `--state` names a state file to read the batch from
    when neither this worktree's nor the first one's has it. The one write it
    makes: `.report.md` into the repository's info/exclude when missing
    (untracked; a worktree cut before the .gitignore entry needs it)."""
    batch_id = report = state_path = None
    it = iter(argv)
    for a in it:
        if a in ('--batch', '--report', '--state'):
            v = next(it, None)
            if v is None:
                print('wave selfcheck: %s needs a value' % a, file=sys.stderr)
                return 2
            if a == '--batch':
                batch_id = v
            elif a == '--report':
                report = v
            else:
                state_path = v
        else:
            print('wave selfcheck: unknown option %s' % a, file=sys.stderr)
            return 2
    if not batch_id:
        print('wave selfcheck: --batch <id> is required', file=sys.stderr)
        return 2
    top = subprocess.run(['git', 'rev-parse', '--show-toplevel'], capture_output=True, text=True)
    worktree = top.stdout.strip() if top.returncode == 0 and top.stdout.strip() else os.getcwd()
    _exclude_draft(worktree)
    if report is None and os.path.isfile(os.path.join(worktree, REPORT_DRAFT)):
        report = os.path.join(worktree, REPORT_DRAFT)
    if report is None:
        print('no report: write %s first or pass --report' % REPORT_DRAFT, file=sys.stderr)
        return 2
    try:
        if report == '-':
            text = sys.stdin.read()
        else:
            with open(report, encoding='utf-8') as f:
                text = f.read()
    except OSError as e:
        print('wave selfcheck: cannot read the report (%s)' % e, file=sys.stderr)
        return 2
    problems = selfcheck(batch_id, worktree, text, state_path=state_path)
    if not problems:
        print('selfcheck: OK')
        return 0
    for p in problems:
        print(p)
    return 1

REVIEW_BRIEF = """# Review of wave-1 batch {id}

You review batch {id} on branch `{branch}`; your cwd is its worktree.
Do not edit, commit or push anything.

The review package is `git diff {base}...{branch}`, written to:

    {diff}

The batch's verdicts (review reason: {why}):

{verdicts}

Check every fork and neither verdict against its deciding case and its
evidence file (`docs/evidence/<ADDRESS>.md`), and that every adopted fork
body carries its `Imported-From:` trailer. Put each finding that must be
fixed before the batch closes on its own line starting exactly
`BLOCKING:`, followed by the address, the file and what to change.
Anything else is advisory: write it without that prefix. When nothing
blocks, say so in one line.
"""

RESUME_PREAMBLE_PROBES = """Resume: batch {id} {why}. This run takes the batch up again in
the same worktree, on `{branch}`. The work done so far is on that branch:
`git log {base}..{branch}`, and `git status` for anything not committed
(a `{wip}` commit, if one is still there, holds what a park saved). Build on it: do not redo
finished work, and do not rewrite that history.{reset}

Your report still carries the full `## Candidates` section: one
`candidate <0xADDR> fork|neither <reason>` line for every row of the batch.
No `## Verdicts` section: the driver decides every row.

The original brief follows.

---

"""


RESUME_PREAMBLE = """Resume: batch {id} {why}. This run takes the batch up again in
the same worktree, on `{branch}`. The work done so far is on that branch:
`git log {base}..{branch}`, and `git status` for anything not committed
(a `{wip}` commit, if one is still there, holds what a park saved). Build on it: do not redo
finished work, and do not rewrite that history.{reset}

Verdicts already recorded in the wave state:

{verdicts}

Your report still carries the full `## Verdicts` section: one line for
every row you decide, the rows you decided before this resume included.
A row you decided `ours` before this resume although your reading says fork
or neither goes under ## Undecided now (rule 13): keep our body and drop its
verdict line.

The original brief follows.

---

"""


class StatusError(ValueError):
    """The driver status file cannot be read: corrupt, torn, or not a
    status object.  The message names the file."""


class _StageError(Exception):
    """A stage of one batch failed: run_batch turns it into that batch's
    exception, never a traceback."""

    def __init__(self, stage, problem):
        super().__init__(problem)
        self.stage = stage
        self.problem = problem


class _LaunchError(Exception):
    """A worker, a reviewer or a check step could not be launched in an
    existing worktree (ruling R45): not the batch's fault, so the run
    halts with the batch parked."""

    def __init__(self, cwd, error):
        super().__init__(error)
        self.cwd = cwd
        self.error = error


def _launch_failed(label, cwd, e):
    """The error for an OSError raised while launching something in cwd:
    a missing cwd (the batch worktree vanished) is the batch's `worktree`
    exception; anything else (a wrapper not on PATH, ...) halts the run."""
    if not os.path.isdir(cwd):
        return _StageError('worktree', '%s could not run: the worktree %s is '
                           'gone (%s); run `git worktree prune`, then `git '
                           'worktree add <path> <branch>`, then clear this '
                           'exception' % (label, cwd, e))
    return _LaunchError(cwd, '%s: %s' % (label, e))


def read_status(path):
    """The status file as it is on disk now, every key present ({}
    defaults when there is no file).  The coordinator changes it through
    `--clear`, so the driver never keeps a copy."""
    for attempt in range(3):
        try:
            with open(path, encoding='utf-8') as f:
                status = json.load(f)
            break
        except FileNotFoundError:
            status = {}
            break
        except ValueError as e:
            if attempt == 2:
                raise StatusError('%s: not valid JSON (%s)' % (path, e)) from None
            time.sleep(0.2)   # a torn read of a file edited in place
        except OSError as e:
            raise StatusError('%s: unreadable (%s)' % (path, e)) from None
    if not isinstance(status, dict):
        raise StatusError('%s: not a driver status file (not a JSON object)'
                          % path)
    for key, empty in (('exceptions', {}), ('batches', {}), ('pools', {}),
                       ('running', {}),
                       ('current', None), ('parked', None),
                       ('stopped', None), ('stalled', None),
                       ('halted', None)):
        status.setdefault(key, empty)
        if isinstance(empty, dict) and not isinstance(status[key], dict):
            raise StatusError('%s: %r is not a JSON object' % (path, key))
    return status


def status_lock(status_path):
    """The flock on the sidecar `<status_path>.lock` that every
    read-modify-write of the status file holds, the driver's and
    `--clear`'s (ruling R37): the read, the change and the replace only."""
    return _tool('filelock').hold(status_path + '.lock', 'wave.py run')


def clear_exception(status_path, batch_id, rerun=False, rerun_frida=False):
    """Remove status['exceptions'][batch_id] under the status lock:
    (0, the removed entry), or (2, None) when there is no such entry.
    StatusError when the file cannot be read.  The removed stage, problems,
    `rerun` and `rerun_frida` are kept under status['cleared'][batch_id] for
    the driver's retry (Task 25: a checks or review exception is re-checked,
    not re-run, unless `rerun`; Task 63: a record or close exception on the
    import queue re-runs the batch's Frida phase first, only when
    `rerun_frida`); the reserved `first` entry keeps none."""
    if not os.path.isfile(status_path):
        return 2, None
    with status_lock(status_path):
        status = read_status(status_path)
        if batch_id not in status['exceptions']:
            return 2, None
        entry = status['exceptions'].pop(batch_id)
        if batch_id != BASE_TESTS_KEY and isinstance(entry, dict):
            status.setdefault('cleared', {})[batch_id] = {
                'stage': entry.get('stage'), 'rerun': bool(rerun),
                'rerun_frida': bool(rerun_frida),
                'problems': list(entry.get('problems') or [])}
        status['updated_at'] = _now()
        _tool('wave')._save_json(status_path, status)
    return 0, entry


def _now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec='seconds')


def _one_line(text, limit=300):
    s = ' '.join(str(text).split())
    return s if len(s) <= limit else s[:limit - 3] + '...'


def _side_case(v):
    """(side, case) from a wave-state verdict dict or a report tuple."""
    if isinstance(v, dict):
        return v.get('side'), v.get('case') or ''
    return v[0], v[1]


def ledger_entry(batch, verdicts, classes, review, commits, credits, now=None,
                 ignored=None, pool=None, wall=None, tokens=None,
                 tool_calls=None, transcript=None, undecided=None, gate=None):
    """The templated progress.md entry for one batch.

    batch carries 'id'; verdicts maps address to a wave-state verdict or a
    (side, case) tuple; classes maps class to row count; review is
    (reviewed, outcome); credits None prints as 'credits unknown';
    ignored maps address to the (side, case) of a worker line set aside
    because the row was triage-decided (a line only when there are any).
    pool, wall (seconds), tokens ({input, cached, output}) and tool_calls
    each add a line when given (rulings R10, R61); undecided maps address
    to the (side, reason) of a row left for a probe (ruling R73); gate
    ('full', 'docs-only' or 'unknown', from the batch) adds a `gate:` line."""
    wave = _tool('wave')
    known = _tool('triage').CLASSES
    order = [c for c in known if c in classes] + sorted(
        c for c in classes if c not in known)
    sides = {s: 0 for s in wave.SIDES}
    deciding = {}
    for v in verdicts.values():
        side, case = _side_case(v)
        sides[side] = sides.get(side, 0) + 1
        k = wave.deciding_kind(case)
        deciding[k] = deciding.get(k, 0) + 1
    reviewed, outcome = review
    lines = [
        '## driver %s (%s)' % (batch['id'], now or _now()),
        '',
        '- rows by class: ' + (', '.join('%s %d' % (c, classes[c]) for c in order)
                               or 'none'),
        '- verdicts: fork %d, ours %d, neither %d' % (
            sides['fork'], sides['ours'], sides['neither']),
    ] + ([
        '- ignored for triage-decided rows: ' + ', '.join(
            '%s %s' % (a, _side_case(v)[0]) for a, v in sorted(ignored.items())),
    ] if ignored else []) + ([
        '- undecided (probe needed): ' + ', '.join(
            '%s %s' % (a, _side_case(v)[0]) for a, v in sorted(undecided.items())),
    ] if undecided else []) + [
        '- deciding evidence: ' + ', '.join(
            '%s %d' % (k, deciding.get(k, 0)) for k in wave.DECIDING_ORDER),
        '- review: %s, %s' % ('yes' if reviewed else 'no', outcome),
        '- commits %s' % commits,
        '- credits unknown' if credits is None else '- credits %.0f' % credits,
    ]
    if gate:
        lines.append('- gate: %s' % gate)
    if pool:
        lines.append('- pool %s' % pool)
    if wall is not None:
        lines.append('- wall %ds' % round(wall))
    if tokens is not None:
        lines.append('- tokens input %d, cached %d, output %d' % (
            tokens['input'], tokens['cached'], tokens['output']))
    if tool_calls is not None:
        lines.append('- tool calls %d' % tool_calls)
    lines += ['- ' + t for t in transcript or ()]
    return '\n'.join(lines) + '\n'


def dry_report(batch):
    """The dry run's canned report: every row a worker must decide (class
    ambiguous, or no class and no verdict yet), all ours, case dry-<addr>."""
    decided = batch.get('verdicts', {})
    if any(r.get('reading') for r in batch.get('rows', [])):
        # a probe batch (Task 28): nothing adopted, every row reported as not built
        return '\n'.join(['# dry run', '', '## Candidates', ''] + [
            'candidate %s neither dry run: nothing adopted' % r['address']
            for r in batch.get('rows', [])]) + '\n'
    lines = ['# dry run', '', '## Verdicts', '']
    for r in batch.get('rows', []):
        cls = r.get('class')
        if cls == 'ambiguous' or (cls is None and r['address'] not in decided):
            lines.append('verdict %s ours case dry-%s' % (r['address'], r['address']))
    return '\n'.join(lines) + '\n'


def blocking_findings(report):
    """The reviewer's lines that start BLOCKING: (one list marker allowed)."""
    out = []
    for raw in (report or '').splitlines():
        line = raw.strip()
        if BLOCKING_RE.match(line):
            out.append(MARKER_RE.sub('', line, count=1).strip())
    return out


BASE_TESTS_LOG = 'base-tests-%s.log'     # per queue: two drivers share the dir
BASE_TESTS_TIMEOUT = 600
RECHECK_STAGES = ('checks', 'review', 'reviewer', 'reviewer-2')   # a cleared one is re-checked
# Problems only the coordinator can clear: never a fix round (Task 26).
COORDINATOR_PROBLEMS = ('first conflicts with this branch in ',
                        'first could not be merged into this branch')
WAITING_LANDING_NOTE = 'waiting for the landing lock'
UNCHANGED_PROBLEM = 'unchanged after recheck; fix by hand or clear with --rerun'
RECHECK_LEDGER = ('retried at its recheck stage: last report re-checked; '
                  'the worker is not run again')
BASE_TESTS_KEY = 'first'     # the reserved exceptions key (not a batch id)


def run_base_tests(root, log_path):
    """(ok, detail): detail is the failure count, or text for a timeout.
    A stop that kills the run raises _Stop (never a red base).  `python3 -I tests/test_tools.py` in root, output to
    log_path, under the gate's memory cap (tools/gates.sh) when systemd-run
    is there.  The child sets IWD2R_BASE_TESTS_CHILD, so a suite that built a
    real Driver by mistake would not recurse."""
    if os.environ.get('IWD2R_BASE_TESTS_CHILD'):
        return True, 0
    argv = ['python3', '-I', 'tests/test_tools.py']
    if shutil.which('systemd-run'):
        argv = ['systemd-run', '--user', '--scope', '-q', '-p',
                'MemoryMax=%s' % os.environ.get('IWD2R_TEST_MEMORY_MAX', '8G'),
                '-p', 'MemorySwapMax=0', '--'] + argv
    os.makedirs(os.path.dirname(os.path.abspath(log_path)), exist_ok=True)
    env = dict(os.environ, IWD2R_BASE_TESTS_CHILD='1')
    try:
        r = _run_group(argv, timeout=BASE_TESTS_TIMEOUT, cwd=root, env=env,
                       text=True, errors='replace')
        text, rc = (r.stdout or '') + (r.stderr or ''), r.returncode
    except subprocess.TimeoutExpired:
        text, rc = 'base tests timed out after %d s\n' % BASE_TESTS_TIMEOUT, 1
        with open(log_path, 'w', encoding='utf-8') as log:
            log.write(text)
        return False, 'timed out after %d s' % BASE_TESTS_TIMEOUT
    except (OSError, subprocess.SubprocessError) as e:
        text, rc = 'base tests could not run: %r\n' % (e,), 1
    if _STOPPED.is_set() and rc < 0:
        raise _Stop()
    with open(log_path, 'w', encoding='utf-8') as log:
        log.write(text)
    if rc == 0:
        return True, 0
    m = re.findall(r'FAILED \((.*?)\)', text)
    n = sum(int(x) for x in re.findall(r'(?:failures|errors)=(\d+)', m[-1])) if m else 0
    return False, n or 1


class _BatchContext(threading.local):
    """The fields of the batch a thread is running (a fresh set per
    thread): the Driver exposes them as properties."""

    def __init__(self):
        self.runs = []          # credits of this batch's worker and reviewer runs
        self.worker_ran = []    # this batch's worker runs: really ran
        self.ignored = {}
        self.undecided = {}     # address -> (side, reason) the worker listed (R73)
        self.candidates = {}    # address -> (side, reason) of a probe report's ## Candidates
        self.probe = {}         # address key -> (side, case) the oracle passed
        self.frida_got = {}     # address key -> (status, detail) of its one Frida run
        self.frida_notes = {}   # address key -> [frequency-only notes] (ledger lines)
        self.wave_notes = {}    # address key -> why an equal wave-1 row got no trace evidence
        self.unmeasured_marked = {}  # address -> evidence rel marked unmeasured WSO import
        self.pool = None        # the pool this batch runs on
        self.t0 = None          # clock() at the cut or resume (ruling R61)
        self.usage = {'input': 0, 'cached': 0, 'output': 0, 'tool_calls': 0}
        self.final = None       # the figures once the ledger has used them
        self.transcript_lines = []   # ledger lines about this batch's transcripts


def _per_thread(name):
    return property(lambda self: getattr(self._ctx, name),
                    lambda self, v: setattr(self._ctx, name, v))


class Driver:
    """The batch loop: cut and triage, brief, worker, mechanical checks
    (one fix round), review when it can matter (one fix round), record,
    close, push, ledger.  A failure the driver cannot settle is an
    exception: written to the status file and the ledger, sent to the AoE
    session, and the loop goes on with the next batch.

    The wave functions are attributes, so tests replace them.  Worker
    pools (Task 15) share the batches: a batch goes to the first pool with
    a free worker (`pick_pool`), each busy worker is one thread, and every
    wave-state mutation and repo-level git operation runs under the one
    driver-wide `_lock` (ruling R62); only the worker and reviewer runs,
    the oracle and Frida phases and the mechanical checks run in parallel.
    The per-batch fields (`_runs`, `_probe`, ...) are thread-local.
    `budget` is in credits over all pools: before each new batch the run
    stops when what it spent plus the next batch's estimate would pass it
    (ruling R38); `budgets` caps single pools.  A worker's 429 parks its
    pool, the WIP committed in the batch worktree; the next run resumes
    that batch on that pool first (ruling R39).  `queue` picks the state
    file's schema twin: wave1 or import (ruling R11)."""

    def __init__(self, repo, state_path, worker, reviewer, ledger_path,
                 status_path, aoe_session=None, seed=DEFAULT_SEED, budget=None,
                 workers=1, *, push=None, send=None, dry_run=False, gate=True,
                 base='first', log=None, frida=False, frida_map=None,
                 rerun_frida=False,
                 oracle_run=None, frida_run=None, frida_build=None, pools=None,
                 workers_by_pool=None, reviewers_by_pool=None, budgets=None,
                 queue='wave1', runner=None, clock=None, max_batches=None,
                 frida_cache=True, base_tests=True, base_tests_run=None,
                 size=None):
        wave = _tool('wave')
        if queue not in wave.QUEUES:
            raise ValueError('unknown queue %r (one of %s)' % (
                queue, ', '.join(wave.QUEUES)))
        self.repo = repo
        self.frida_cache = frida_cache
        # Task 23: the tool test suite runs on the base worktree at start and
        # before each new cut; `base_tests_run(root, log_path)` returns
        # (ok, failures) and is what tests replace.
        self.base_tests = base_tests
        self.base_tests_run = (run_base_tests if base_tests_run is None
                               else base_tests_run)
        self.state_path = state_path
        self.queue = queue
        self.kind = wave.QUEUES[queue]['kind']
        # One pool by default, named glm; `worker` and `reviewer` serve
        # every pool that workers_by_pool / reviewers_by_pool leaves out.
        self.pools = list(pools) if pools else [Pool(
            'glm', 'claude-glm', 'glm-5.3-flash[1m]', workers,
            'glm-5.3-flash[1m]')]
        if len({p.name for p in self.pools}) != len(self.pools):
            raise ValueError('two pools have the same name')
        self.workers_by_pool = dict(workers_by_pool or {})
        self.reviewers_by_pool = dict(reviewers_by_pool or {})
        for p in self.pools:
            self.workers_by_pool.setdefault(p.name, worker)
            self.reviewers_by_pool.setdefault(p.name, reviewer)
        self.budgets = dict(budgets or {})
        for name in self.budgets:
            pool = next((p for p in self.pools if p.name == name), None)
            if pool is None:
                raise ValueError('--budget-%s: no pool named %s' % (name, name))
            if not pool_is_glm(pool):
                raise ValueError(
                    '--budget-%s is refused: pool %s runs %s, which Z.ai '
                    'credits do not price (only GLM turns are priced); '
                    'budget a GLM pool, or cap the run with --budget over '
                    'the GLM pools' % (name, name, pool.model))
        self.ledger_path = ledger_path
        self.status_path = status_path
        self.driver_dir = os.path.dirname(os.path.abspath(status_path))
        self.aoe_session = aoe_session
        self.seed = seed
        self.budget = budget
        self.workers = (workers if pools is None
                        else sum(p.workers for p in self.pools))
        self.dry_run = dry_run
        self.max_batches = max_batches
        self.gate = gate
        self.base = base
        self.size = wave.DEFAULT_SIZE if size is None else size
        if self.size < 1:
            raise ValueError('--size must be at least 1')
        self.category = wave.QUEUES[queue]['category']
        docs = os.path.join(repo, 'docs', 'wso')
        self.disagreements_path = os.path.join(docs, 'disagreements.json')
        self.triage_path = os.path.join(docs, 'triage.json')
        self.template_path = os.path.join(
            docs, os.path.basename(wave.QUEUES[queue]['template']))
        self.report_path = os.path.join(docs, 'xrec-report.md')
        self.probe_path = os.path.join(docs, 'probe-list.json')
        self.import_list_path = os.path.join(repo, wave.IMPORT_LIST_REL)
        self.calibration_path = os.path.join(docs, 'truth-calibration.json')
        truth = _tool('truth')
        self.frida = frida
        # Task 63: --rerun-frida, and nothing else (a plain --clear keeps
        # R71), makes a record/close retry re-run the batch's Frida phase.
        self.rerun_frida = rerun_frida
        self.exports_dir = None
        # The host taskkill of the sandbox, behind one seam: tests replace it.
        self._taskkill = _tool('winsandbox').taskkill_sandbox
        self.held_out = tuple(_tool('truth').PHASE_HELD_OUT)   # scenarios the phase skips for now
        self.frida_map = frida_map
        self.oracle_run = truth.run_oracle if oracle_run is None else oracle_run
        self.frida_run = truth.frida_run if frida_run is None else frida_run
        # The per-batch x86 build the phase measures (spec B); injectable.
        self.frida_build = globals()['frida_build'] if frida_build is None else frida_build
        self.next_batch = wave.next_batch
        self.recompare = wave.recompare
        self._recompared = False
        self.render_brief = wave.render_brief
        self.record_verdict = wave.record_verdict
        self.close_batch = wave.close_batch
        self.sync_base = wave._sync_base
        self.write_scorecard = wave.write_scorecard
        self.cut_worktree = wave._cut_worktree
        self.apply_triage = wave.apply_triage
        self.worktree_for = wave.worktree_for
        self.load_state = wave.load_state
        self.mechanical_checks = mechanical_checks
        self.review_needed = review_needed
        self.push = self._git_push if push is None else push
        self.send = self._aoe_send if send is None else send
        self.log = (lambda m: print(m, flush=True)) if log is None else log
        self._read_status()   # a corrupt status file refuses here
        # Ruling R62: the one lock over every wave-state mutation and
        # repo-level git operation; never taken twice by one thread.
        self._lock = threading.Lock()
        # Git writes in progress (their thread idents), so a stop waits for
        # them: a signal must not land mid commit or fast-forward.
        self._write_cv = threading.Condition(threading.RLock())
        self._writers = {}         # thread ident -> nesting depth
        self._stop_deferred = None    # ident of the thread whose write defers the stop
        self._ctx = _BatchContext()
        self._runner = runner      # runs a callable on its own thread; None: threads
        self._clock = time.monotonic if clock is None else clock
        self._started = {}         # batch id -> clock() at its cut
        self._busy = {p.name: 0 for p in self.pools}
        self._cursor = 0           # pick_pool's round-robin position
        self._parked_now = set()   # pools that ended a batch parked this run
        self._threads = []
        self._pool_batch_credits = {p.name: [] for p in self.pools}
        self._imported = {}
        self._stopping = False     # SIGTERM or SIGINT arrived (ruling R72)
        self._retry_stage = {}     # cleared batch id -> the stage it failed at
        self._retry_kind = {}      # cleared batch id -> 'recheck' | 'rerun' (checks/review only)
        self._retry_problems = {}  # cleared batch id -> the problems its exception recorded
        self._retry_frida = {}     # cleared batch id -> --rerun-frida was on at --clear time
        self._halt = False
        self._batch_credits = []   # this run's batches: credits or None
        self._no_transcript = 0    # batch exceptions in a row without one
        self._began = 0            # batches started in this run (--max-batches)

    _runs = _per_thread('runs')
    _worker_ran = _per_thread('worker_ran')
    _ignored = _per_thread('ignored')
    _undecided = _per_thread('undecided')
    _kept = _per_thread('kept')
    _candidates = _per_thread('candidates')
    _probe = _per_thread('probe')
    _frida_got = _per_thread('frida_got')
    _frida_notes = _per_thread('frida_notes')
    _wave_notes = _per_thread('wave_notes')
    _unmeasured_marked = _per_thread('unmeasured_marked')

    @property
    def worker(self):
        return self.workers_by_pool[self.pools[0].name]

    @worker.setter
    def worker(self, value):
        self.workers_by_pool[self.pools[0].name] = value

    @property
    def reviewer(self):
        return self.reviewers_by_pool[self.pools[0].name]

    @reviewer.setter
    def reviewer(self, value):
        self.reviewers_by_pool[self.pools[0].name] = value

    def _pool_name(self):
        """The pool the current batch runs on."""
        return self._ctx.pool or self.pools[0].name

    # -- status, ledger, results ----------------------------------------

    def _read_status(self):
        """The status file as it is on disk now (the coordinator's
        `--clear` changes it between the driver's writes)."""
        return read_status(self.status_path)

    def _update_status(self, change):
        """Read-modify-write under the status lock: re-read the file,
        apply only this change, write it atomically.  What the
        coordinator removed stays removed."""
        with status_lock(self.status_path):
            status = self._read_status()
            change(status)
            if self.dry_run:
                status['dry_run'] = True
            status['updated_at'] = _now()
            _tool('wave')._save_json(self.status_path, status)
        return status

    def _stage(self, batch_id, stage):
        current = {'batch': batch_id, 'stage': stage, 'at': _now()}

        def apply(s):
            s['current'] = current
            s['running'][batch_id] = dict(current, pool=self._pool_name())
        self._update_status(apply)
        self.log('%s: %s' % (batch_id, stage))

    def _figures(self):
        """{wall, tokens, tool_calls} of the batch this thread runs
        (ruling R61), fixed at the first call so the ledger entry and the
        status agree; None for a batch that was never started."""
        if self._ctx.t0 is None:
            return None
        if self._ctx.final is None:
            u = self._ctx.usage
            self._ctx.final = {
                'wall': max(0.0, self._clock() - self._ctx.t0),
                'tokens': {k: u[k] for k in ('input', 'cached', 'output')},
                'tool_calls': u['tool_calls']}
        return self._ctx.final

    def _finish(self, batch_id, outcome, change=None, **extra):
        """Record the batch's outcome; `change` edits the status in the
        same write.  Any outcome but another park ends the batch's park."""
        entry = dict(outcome=outcome, credits=self._credits(), at=_now(), **extra)
        figures = self._figures()
        if figures:
            entry.update(wall=round(figures['wall'], 1),
                         tokens=dict(figures['tokens']),
                         tool_calls=figures['tool_calls'])

        def apply(s):
            old = s['batches'].get(batch_id)
            for field in ('oracle_findings', 'frida_written', 'retry_kind'):
                if isinstance(old, dict) and old.get(field):
                    entry[field] = old[field]
            s['batches'][batch_id] = entry
            s['running'].pop(batch_id, None)
            # current: the latest batch still in flight, if any.
            left = sorted(s['running'].items(), key=lambda kv: kv[1].get('at', ''))
            s['current'] = ({'batch': left[-1][0], 'stage': left[-1][1].get('stage'),
                             'at': left[-1][1].get('at')} if left else None)
            if outcome != 'parked':
                _unpark_everywhere(s, batch_id)
            if change:
                change(s)
        self._update_status(apply)

    def _append_ledger(self, text):
        with self._lock:
            os.makedirs(os.path.dirname(os.path.abspath(self.ledger_path)),
                        exist_ok=True)
            sep = '\n' if os.path.isfile(self.ledger_path) and os.path.getsize(
                self.ledger_path) else ''
            with open(self.ledger_path, 'a', encoding='utf-8') as f:
                f.write(sep + text.rstrip('\n') + '\n')

    def _credits(self):
        if not self._runs or any(c is None for c in self._runs):
            return None
        return sum(self._runs)

    def _run(self, worker, brief, cwd, batch_id, label):
        """One worker run; the brief, report and result land in
        <driver folder>/<batch_id>/<label>.*"""
        if self._stopping:     # stopped before this launch: start nothing
            raise _Stop()
        anchors = self._anchors(cwd)
        try:
            res = worker.run(brief, cwd)
        except OSError as e:
            raise _launch_failed(label, cwd, e) from None
        if self._stopping:     # killed with its driver: nothing to record
            raise _Stop()
        self._runs.append(res.credits)
        if label.startswith('worker'):
            self._worker_ran.append(worker_ran(res))
        if res.transcript:
            try:
                tokens, calls = usage_all(res.transcript)
            except OSError:
                tokens, calls = None, 0
            if tokens:
                u = self._ctx.usage
                for k in ('input', 'cached', 'output'):
                    u[k] += tokens[k]
                u['tool_calls'] += calls
        folder = os.path.join(self.driver_dir, batch_id)
        os.makedirs(folder, exist_ok=True)
        for name, text in (('brief.md', brief), ('report.md', res.report or '')):
            with open(os.path.join(folder, '%s.%s' % (label, name)), 'w',
                      encoding='utf-8') as f:
                f.write(text)
        with open(os.path.join(folder, label + '.json'), 'w',
                  encoding='utf-8') as f:
            json.dump(res._asdict(), f, indent=2, sort_keys=True)
            f.write('\n')
        self.log('%s: %s exited %s, credits %s' % (
            batch_id, label, res.exit_code,
            'unknown' if res.credits is None else '%.1f' % res.credits))
        # Last, after the bookkeeping above: a transcript violation is an
        # exception, but the run it found still counts as a worker run
        # for the breaker and its evidence is saved (ruling R68).
        self._scan_transcript(batch_id, label, res, anchors, cwd)
        return res

    def _anchors(self, cwd):
        """The commit a worker must not lose: the branch tip before it starts
        (start_tip; it holds the driver's own `frida: trace evidence` commits
        too, being their descendant).  [] when cwd is no repository."""
        try:
            r = subprocess.run(['git', '-C', cwd, 'rev-parse', '--verify', 'HEAD'],
                               capture_output=True, text=True)
        except OSError:
            return []
        return [r.stdout.strip()] if r.returncode == 0 and r.stdout.strip() else []

    def _only_own_commits(self, cwd, anchors):
        """True when every anchor is still an ancestor of the branch tip."""
        if not anchors:
            return False
        try:
            return all(subprocess.run(
                ['git', '-C', cwd, 'merge-base', '--is-ancestor', a, 'HEAD'],
                capture_output=True).returncode == 0 for a in anchors)
        except OSError:
            return False

    def _scan_transcript(self, batch_id, label, res, anchors=(), cwd=None):
        """Every run's transcript, worker, fix rounds and reviewer: a wso/
        ref put on disk, a launch outside the house runners or a history
        rewrite is a past call nobody can undo, so a `transcript` exception
        with no fix round; a whole-file read or a git read of a wso/ ref is
        a ledger line only; a missing transcript is a ledger line."""
        lines = self._ctx.transcript_lines
        if not res.transcript:
            lines.append('transcript: none (%s)' % label)
            return
        batch = self.load_state(self.state_path)['batches'].get(batch_id) or {}
        covered = sorted({r['file'] for r in batch.get('rows', [])
                          if r.get('file')})
        fatal = []
        for kind, text, f in transcript_findings(res.transcript, covered,
                                              self._is_tracked):
            if kind == 'read':
                line = 'transcript: whole read of %s' % f
                if line not in lines:
                    lines.append(line)
            elif kind == 'wso-read':
                if text not in lines:
                    lines.append(text)
            elif kind == 'unreadable':
                lines.append('transcript: unreadable (%s)' % label)
            elif kind == 'history' and (f or '').startswith('amend:') and cwd and \
                    self._only_own_commits(cwd, list(anchors)):
                # an amend of the worker's own commit: nothing the driver or
                # an earlier run committed moved
                lines.append('transcript: rewrote only its own commits (%s)' % f[6:])
            else:
                fatal.append('%s (%s)' % (text, label))
        if fatal:
            raise _StageError('transcript', ' | '.join(fatal))

    # -- outcomes ---------------------------------------------------------

    def _exception(self, batch_id, stage, problems):
        at = _now()
        problems = [str(p) for p in problems if str(p).strip()] or ['(no detail)']
        exc = {'stage': stage, 'problems': problems, 'at': at}
        self._finish(batch_id, 'exception', stage=stage,
                     change=lambda s: s['exceptions'].__setitem__(batch_id, exc))
        if stage in GLOBAL_STAGES:
            self._halt = True
        n = len(problems)
        line = ('driver %s: exception at %s (%s): %d problem%s, first: %s; '
                'the full list is in %s' % (
                    batch_id, stage, at, n, '' if n == 1 else 's',
                    _one_line(problems[0], 200), self.status_path))
        self._append_ledger(line)
        self.log(line)
        if not self.dry_run:
            try:
                self.send(self._exception_line(batch_id, stage))
            except Exception as e:   # a failed send must not stop the loop
                self.log('send failed: %r' % (e,))
        return 'exception'

    def _exception_line(self, batch_id, stage):
        """The one line sent over aoe (ruling R71): the batch, the stage
        and where to read more; never problem text, which comes from a
        worker's work."""
        return 'driver: %s exception at %s; see %s' % (
            batch_id, stage, self.status_path)

    def _commit_wip(self, worktree, message=WIP_MESSAGE):
        """Commit what the batch worktree holds uncommitted, as
        `git add -A && git commit -m WIP_MESSAGE` (ruling R39): (sha, None),
        (None, None) when it is clean, or (None, why) when nothing was
        committed.  Never in the driver's own worktree."""
        if not worktree:
            return None, 'no batch worktree'
        if os.path.realpath(worktree) == os.path.realpath(self.repo):
            return None, ("%s is the driver's own worktree, not a batch "
                          "worktree" % worktree)

        def git(*args):
            return subprocess.run(['git', '-C', worktree, *args],
                                  capture_output=True, text=True)
        st = git('status', '--porcelain')
        if st.returncode != 0:
            return None, 'git status failed: %s' % _one_line(st.stderr)
        if not st.stdout.strip():
            return None, None
        for args in (('add', '-A'), ('commit', '-q', '-m', message)):
            r = git(*args)
            if r.returncode != 0:
                return None, 'git %s failed: %s' % (
                    args[0], _one_line((r.stderr or '') + (r.stdout or '')))
        return git('rev-parse', 'HEAD').stdout.strip() or None, None

    def _park(self, batch_id, reason, worktree):
        """Commit the WIP in the batch worktree, write the pool's
        `parked`; the pool takes no more batches, the run exits 3 once the
        other pools are done, and the next run resumes this batch first."""
        wip, err = self._commit_wip(worktree)
        extra = {}
        if wip:
            extra['wip'] = wip
        if err:
            extra['wip_error'] = err
            self.log('%s: WIP not committed: %s' % (batch_id, err))
        parked = {'at': _now(), 'reason': reason}
        name = self._pool_name()

        def mark(s):
            _unpark_everywhere(s, batch_id)
            info = s['pools'].setdefault(name, {})
            m = parked_map(info.get('parked'))
            m[batch_id] = parked
            _set_parked(info, m)
        self._finish(batch_id, 'parked', change=mark, **extra)
        self.log('%s: parked (%s)%s' % (
            batch_id, reason, '; WIP committed as %s' % wip[:8] if wip else ''))
        return 'parked'

    def _settle(self, batch_id, res, label, worktree):
        """'parked' or 'exception' when a run gave nothing to check.  An
        auth or billing error parks the batch and halts the run (R46)."""
        if res.timed_out:
            return self._park(batch_id, 'timeout', worktree)
        if res.api_error:
            reason = 'api error: %s' % res.api_error
            out = self._park(batch_id, reason, worktree)
            self._halt_run(reason)
            return out
        if res.rate_limited:
            return self._park(batch_id, '%s rate limited' % label, worktree)
        if not (res.report or '').strip():
            return self._exception(
                batch_id, 'review' if label.startswith('reviewer') else 'worker',
                ['%s exited %s without a report' % (label, res.exit_code)])
        return None

    # -- the stages ---------------------------------------------------------

    def _check(self, batch_id, worktree, report):
        """(verdicts, problems) for a report: the parse, then the
        mechanical checks.  Lines for rows whose recorded verdict is a
        triage-* case are set aside in self._ignored (ruling R33): never
        recorded, never checked, never a reason to review."""
        self._ignored = {}
        self._undecided = {}
        self._kept = {}
        self._candidates = {}
        try:
            parsed = parse_report(report)
        except ReportError as e:
            return {}, ['report: %s; fix that line under ## Verdicts (one '
                        '"verdict <0xADDR> <fork|ours|neither> case <id>" '
                        'per row; ## Verdicts holds verdict lines only, put '
                        'notes under ## Notes)' % e]
        try:
            listed = parse_undecided(report)
        except ReportError as e:
            return {}, ['report: %s; fix that line under ## Undecided (one '
                        '"undecided <0xADDR> <fork|neither> <one-line '
                        'reason>" per row)' % e]
        if self.kind == 'import':
            try:
                self._kept = parse_kept(report)
            except ReportError as e:
                return {}, ['report: %s; fix that line under ## Kept (one "kept <0xADDR> '
                            '<one-line reason>" per row, for a base-body row you leave as '
                            'ours; never also under ## Imported)' % e]
        if self.kind == 'probes':
            try:
                self._candidates = parse_candidates(report)
            except ReportError as e:
                return {}, ['report: %s; fix that line under ## Candidates (one "candidate '
                            '<0xADDR> <fork|neither> <one-line reason>" per row)' % e]
        self._stage(batch_id, 'checks')
        if not self.dry_run:
            merge_problems = self._merge_before_checks(batch_id, worktree)
            if merge_problems:
                return {}, merge_problems
        sections = []
        if self.kind == 'import':
            # before any outcome is touched: a section mistake must not cost
            # the kept outcome the phase recorded (the fix round puts it right);
            # its problems join the other checks (a batch gets one fix round)
            sections = [] if self.dry_run else self._section_problems(
                batch_id, worktree, report)
            if not sections:
                self._clear_kept_outcomes(batch_id, self._kept)
        if self.kind == 'verdicts' and not self.dry_run:
            self._withdraw_changed(batch_id, worktree)
        batch = self.load_state(self.state_path)['batches'][batch_id]
        key = _tool('wave')._addr_key
        auto = {key(a) for a, v in batch.get('verdicts', {}).items()
                if _side_case(v)[1].startswith('triage-')}
        verdicts = {}
        for addr, sc in parsed.items():
            (self._ignored if key(addr) in auto else verdicts)[addr] = sc
        if self._ignored:
            self.log('%s: ignored for triage-decided rows: %s' % (
                batch_id, ', '.join(sorted(self._ignored))))
        # Ruling R48: a probe pass is the verdict, whatever the report says.
        for addr, sc in self._probe_verdicts(batch).items():
            if addr in verdicts and verdicts[addr] != sc:
                self.log('%s: %s decided by the oracle probe (%s); the '
                         'report line is replaced' % (batch_id, addr, sc[1]))
            verdicts[addr] = sc
        # A row the driver's own probe decided, or triage decided, is not
        # undecided whatever the report says.
        for addr in sorted(listed):
            if key(addr) in auto:
                del listed[addr]
        probed = {key(a) for a in self._probe_verdicts(batch)}
        for addr in sorted(listed):
            if key(addr) in probed:
                self.log('%s: %s decided by the oracle probe; its undecided '
                         'line is dropped' % (batch_id, addr))
                del listed[addr]
        self._undecided = {} if self.kind in ('import', 'probes') else dict(listed)
        try:
            spelling = self._canonical_evidence_names(
                batch_id, worktree, batch, verdicts, dict(listed, **self._candidates))
            kw = {'kind': 'import'} if self.kind == 'import' else {}
            if self.kind == 'probes':
                kw = {'kind': 'probes', 'candidates': dict(self._candidates)}
            kw['probe'] = sorted(self._probe)
            kw['undecided'] = sorted(listed)
            kw['frida_lines'] = self._frida_lines(batch_id)
            if self.kind == 'import':
                kw['kept'] = sorted(self._kept)
            return verdicts, sections + spelling + self.mechanical_checks(
                dict(batch, id=batch_id), worktree, verdicts, **kw)
        except OSError as e:
            raise _launch_failed('the mechanical checks', worktree, e) from None

    def _canonical_evidence_names(self, batch_id, worktree, batch, verdicts, listed):
        """Rename the evidence files of the batch's rows (and the dependencies
        the report names) that carry a misspelt name (`0x78A2F0.md`, lower
        case) to `docs/evidence/<ADDR>.md`, one commit for all of them, and
        return the problems: both spellings present is never resolved here.
        Runs before any evidence-existence rule."""
        ce = _tool('check_evidence')
        key = _tool('wave')._addr_key
        addrs = {}
        for a in ([r['address'] for r in batch.get('rows', [])] + list(verdicts)
                  + list(listed) + list(self._kept) + list(batch.get('measured_deps') or {})):
            addrs.setdefault(key(a), a)
        try:
            for _p, a, _n in ce.touched(worktree, self.base):
                addrs.setdefault(key('0x%X' % a), '0x%X' % a)
        except Exception as e:
            self.log('%s: evidence spelling: cannot list the touched functions (%r)'
                     % (batch_id, e))
        problems, moves = [], []
        for k in sorted(addrs):
            rel, old = ce.canonical_evidence(worktree, k)
            odd = ce.misspelt_evidence(worktree, k)
            have = os.path.isfile(os.path.join(worktree, rel))
            if (have and odd) or len(odd) > 1:
                first = [rel] if have else []
                files = first + odd
                problems.append('two evidence files for 0x%s: %s; keep one, delete the %s'
                                % (k.upper(), ' and '.join(files) if len(files) < 3
                                   else ', '.join(files),
                                   'other' if len(files) < 3 else 'others'))
            elif old:
                moves.append((old, rel))
        if not moves:
            return problems

        def git(*args):
            return subprocess.run(['git', '-C', worktree, *args], capture_output=True, text=True)
        done = []
        for old, new in moves:
            r = git('mv', '--', old, new)
            if r.returncode != 0:
                # an untracked (or half-tracked) file: move it, then track the new name
                try:
                    os.rename(os.path.join(worktree, old), os.path.join(worktree, new))
                except OSError as e:
                    problems.append('cannot rename %s to %s (%s); rename it' % (old, new, e))
                    continue
                git('add', '--', new)
                git('rm', '-q', '--cached', '--ignore-unmatch', '--', old)
            done.append((old, new))
            self.log('%s: evidence spelling: %s -> %s' % (batch_id, old, os.path.basename(new)))
        if done:
            paths = []
            for old, new in done:
                paths.append(new)
                # the old name only when HEAD still carries it (a tracked file)
                if git('cat-file', '-e', 'HEAD:' + old).returncode == 0:
                    paths.append(old)
            r = git('commit', '-q', '-m', 'evidence: canonical file names for %d row(s)'
                    % len(done), '--', *paths)
            if r.returncode != 0:
                problems.append('evidence file names not committed: %s'
                                % _one_line((r.stderr or '') + (r.stdout or '')))
        return problems

    # -- measurement (tools/truth.py) -----------------------------------

    def _open_rows(self, batch):
        """The rows a worker must decide: class ambiguous, or no class and no
        verdict yet (the same test as the mechanical checks)."""
        decided = batch.get('verdicts', {})
        return [r for r in batch.get('rows', [])
                if r.get('class') == 'ambiguous' or (
                    'class' not in r and r['address'] not in decided
                    and not r.get('verdict'))]

    def _probe_verdicts(self, batch):
        """{address as in the batch rows: (side, case)} for the rows the
        oracle probe passed (ruling R48: side ours, case probe-<batch>-<addr>)."""
        out = {}
        for r in batch.get('rows', []):
            hit = self._probe.get(_tool('wave')._addr_key(r['address']))
            if hit:
                out[r['address']] = hit
        return out

    def _measured_for(self, addr, side):
        """The `measured` a verdict is recorded with, or None."""
        key = _tool('wave')._addr_key(addr)
        if key in self._probe:
            return 'probe'
        # The Frida phase measures the batch's own build, so an equal row
        # confirms whichever side's body the branch holds.
        if side in ('ours', 'fork') and self._frida_got.get(key, ('',))[0] == 'equal':
            return 'frida'
        return None

    def _oracle_phase(self, batch_id, batch, worktree):
        """Before the worker: draft, build and run an oracle case for the
        open rows that are pure.  Passes become verdicts (self._probe), a fail
        is a finding for the brief, crash and pending leave the row alone.
        Returns the text to put in front of the brief ('' for none).  A batch
        with no pure row skips the build and the run; an oracle that cannot
        run is logged and the batch goes on unmeasured."""
        self._probe = {}
        truth = _tool('truth')
        wave = _tool('wave')
        decided = {wave._addr_key(a) for a in batch.get('verdicts', {})}
        rows = [r for r in self._open_rows(batch)
                if wave._addr_key(r['address']) not in decided]
        carried = self._carry_probe_passes(batch_id, worktree)
        saved = (self._read_status()['batches'].get(batch_id) or {}).get('oracle_findings') or []
        again = ('## Oracle findings from the earlier run\n\n' + '\n'.join(saved) + '\n\n') if saved else ''
        if carried is not None:
            # A resume: the probe file is already committed, so its passes
            # stand and the oracle does not run again (it would rewrite it).
            for k in carried:
                self._probe[k] = ('ours', 'probe-%s-%s' % (batch_id, k.upper()))
            self.log('%s: oracle: %d earlier probe pass(es) carried forward' % (
                batch_id, len(carried)))
            if not carried:
                return again
            return again + ('## Measured before you started\n\nThe oracle probe passed these rows earlier '
                    '(the driver records each as `ours`; write the ## WSO delta sentence `corroborated by WSO at <sha>: ...; case probe-<batch>-<ADDR> decides`, the exact case shown):\n'
                    + ''.join('- 0x%s: case %s\n' % (k.upper(), self._probe[k][1]) for k in sorted(carried))
                    + '\n')

        def read(address):
            text, _err = wave._read_evidence(self.repo, batch['branch'], address)
            return text
        try:
            cases, _why = truth.oracle_candidates(rows, read)
        except Exception as e:
            self.log('%s: oracle path skipped: %s' % (batch_id, _one_line(repr(e), 300)))
            return again
        if not cases:
            return again
        try:
            self._stage(batch_id, 'oracle')
            got = self.oracle_run(batch_id, worktree, cases)
            got = {wave._addr_key(a): v for a, v in got.items()}
        except Exception as e:   # measurement is best effort
            self.log('%s: oracle path skipped: %s' % (batch_id, _one_line(repr(e), 300)))
            self._settle_oracle_files(batch_id, worktree, cases, ())
            return again
        self._settle_oracle_files(batch_id, worktree, cases, [
            c for c in cases if got.get(wave._addr_key(c['address']), ('',))[0] == 'pass'])
        key = wave._addr_key
        passed, failed = [], []
        for c in cases:
            status, detail = got.get(key(c['address']), ('pending', 'no result'))
            self.log('%s: oracle %s %s (%s)' % (batch_id, c['address'], status, detail))
            if status == 'pass':
                case = 'probe-%s-%s' % (batch_id, key(c['address']).upper())
                self._probe[key(c['address'])] = ('ours', case)
                passed.append((c['address'], case, detail))
            elif status == 'fail':
                failed.append((c['address'], c['name'], detail, c))
        lines = []
        if passed:
            lines.append('The oracle probe passed these rows (ours matches the original over a '
                         'swept domain). The driver records each as `ours`, with the case shown; '
                         'write the evidence file with the sentence '
                         '`corroborated by WSO at <sha>: ...; case probe-<batch>-<ADDR> decides` '
                         'in the ## WSO delta section, with the exact case shown:')
            lines += ['- %s: case %s (%s)' % p for p in passed]
        if failed:
            lines.append('The oracle probe FAILED these rows (ours and the original disagree). '
                         'Decide them with that in mind. The failing case is not kept in the '
                         'tree; its source, each integer argument swept over {min, -1, 0, 1, 7, max}:')
            for address, name, detail, c in failed:
                lines.append('- %s %s: case wave/%s/%s, %s' % (
                    address, name, batch_id, key(address)[0:].upper(), detail))
                lines.append('```cpp\n' + truth.draft_case(
                    c['address'], c['name'], c['signature'], batch_id).rstrip() + '\n```')
        if failed:
            texts = [l for l in lines[lines.index(next(x for x in lines if x.startswith('The oracle probe FAILED'))):]]
            self._update_status(lambda s: s['batches'].setdefault(batch_id, {}).__setitem__(
                'oracle_findings', texts))
        text = ('## Measured before you started\n\n' + '\n'.join(lines) + '\n\n') if lines else ''
        return text if failed else again + text

    def _tracked(self, worktree, rel):
        r = subprocess.run(['git', '-C', worktree, 'ls-files', '--error-unmatch', '--', rel],
                           capture_output=True, text=True)
        return r.returncode == 0

    def _carry_probe_passes(self, batch_id, worktree):
        """None when the batch has no committed probe file; else the address
        keys of the passing cases in it."""
        rel = 'oracle/cases/wave-%s.cpp' % batch_id
        if not self._tracked(worktree, rel):
            return None
        try:
            with open(os.path.join(worktree, rel), encoding='utf-8') as f:
                text = f.read()
        except OSError:
            return None
        return set(m.group(1).lower() for m in re.finditer(
            r'ORACLE_CASE\("wave/%s/([0-9A-Fa-f]+)"' % re.escape(batch_id), text))

    def _settle_oracle_files(self, batch_id, worktree, cases, passed):
        """Leave in the batch worktree only what is worth keeping of the
        oracle path's files: the case file with the passing cases (their
        permanent oracle cases) and its oracle/CMakeLists.txt line, committed;
        a failing, crashing or unrun case never stays in the oracle suite, and
        with nothing passing both files are put back."""
        truth = _tool('truth')
        cmake = os.path.join(worktree, 'oracle', 'CMakeLists.txt')
        cases_dir = os.path.join(worktree, 'oracle', 'cases')
        path = os.path.join(cases_dir, 'wave-%s.cpp' % batch_id)

        def git(*args):
            return subprocess.run(['git', '-C', worktree, *args],
                                  capture_output=True, text=True)
        try:
            if not os.path.exists(path):
                return
            if not passed:
                # Tracked files (an earlier committed probe) are restored,
                # never deleted; only an untracked case file is unlinked.
                if self._tracked(worktree, 'oracle/cases/wave-%s.cpp' % batch_id):
                    git('checkout', 'HEAD', '--', 'oracle/cases/wave-%s.cpp' % batch_id,
                        'oracle/CMakeLists.txt')
                else:
                    os.unlink(path)
                    git('checkout', 'HEAD', '--', 'oracle/CMakeLists.txt')
                return
            if len(passed) != len(cases):
                truth.write_cases(batch_id, passed, cases_dir)
            names = ['oracle/cases/wave-%s.cpp' % batch_id, 'oracle/CMakeLists.txt']
            if git('status', '--porcelain', '--', *names).stdout.strip():
                git('add', '--', *names)
                r = git('commit', '-q', '-m', 'truth: oracle probes for %s (pure rows; the driver)' % batch_id,
                        '--', *names)
                if r.returncode != 0:
                    self.log('%s: oracle files not committed: %s' % (
                        batch_id, _one_line((r.stderr or '') + (r.stdout or ''))))
        except OSError as e:
            self.log('%s: oracle files left as they are: %r' % (batch_id, e))

    def _frida_phase(self, batch_id, batch, worktree, verdicts, rows=None, commit=None):
        """After the worker, with --frida: one batched Frida differential run
        (the scenarios the coverage map picks for the batch, see
        _coverage_scenarios) of an x86 build of the batch worktree
        (_frida_target).  Wave 1 measures the open rows the oracle did not
        decide; the build is the branch's, so equal confirms whichever side
        the verdict records (measured frida) and different is a finding for
        that side.  An import batch measures every row not parked plus every
        function the batch touched that carries a `// 0xADDR` marker (a
        dependency imported with a row), except the ones whose evidence
        stands as written (wave.kept_evidence: an oracle case, a fork note
        that names the function, or evidence unchanged since the merge base);
        an equal row's evidence is rewritten to kind: trace and committed on
        the batch branch, and any other outcome (different, not exercised, no
        hook, scenario failed, not measured) is a finding that parks it --
        except a stub or only-theirs row exercised by no scenario (rule 1 of
        the 2026-10-26 ruling): ours never had the body, so the import needs
        no coverage; its evidence is marked unmeasured WSO import and
        committed, and the row closes (wave.py unmeasured-imports lists every
        such row).  A
        phase-wide failure (no build, the run raised, every scenario failed
        with no status on either side, state dumps the replay schema refuses)
        raises _StageError('frida'): the
        batch's exception, nothing measured, parked or rewritten.  It runs
        once per batch, never again unless something it measured changed
        (a retry reuses frida-cache.json, see _frida_cache_key).  Returns the findings."""
        self._frida_got = {}
        self._frida_notes = {}
        self._wave_notes = {}
        self._unmeasured_marked = {}
        self._frida_ran = None
        if not self.frida or self.dry_run:
            return []
        truth = _tool('truth')
        wave = _tool('wave')
        key = wave._addr_key
        importing = self.kind == 'import'
        mb = None
        info = {}
        if importing:
            mb_sha = wave.merge_base_sha(worktree, self.base)
            mb = mb_sha[:12] if mb_sha else self.base
            rows = self._import_rows(batch_id, batch, worktree, mb_sha)
        elif rows is None:   # `rows` given: a remeasure pass names its own (closed) rows
            rows = [r for r in self._open_rows(batch) if key(r['address']) not in self._probe]
        if not rows:
            return []
        run_scenarios, skip = self._coverage_scenarios(batch_id, rows)
        if skip:
            # No scenario of the phase hits any row (the coverage map of the
            # original): no build, no launch; the same shape the run's
            # verdicts have, read by the loop below.
            hit_text = '%s; consulted %s' % (COVERAGE_NOT_HIT, ', '.join(self._frida_consulted))
            self._frida_got = {key(r['address']): (
                ('not measured', COVERAGE_UNHOOKABLE)
                if key(r['address']) in self._frida_unhookable else ('not-exercised', hit_text))
                for r in rows}
            return self._frida_findings(batch_id, worktree, rows, dict(self._frida_got), {},
                                        {}, {}, verdicts, importing, mb, commit)
        sigs = {}
        for r in rows:
            text, _err = wave._read_evidence(self.repo, batch['branch'], r['address'])
            sig = truth.evidence_signature(text) if text else None
            if not sig:
                sig = truth.export_signature(r['address'], self.exports_dir)
                if sig:
                    self.log('%s: frida: %s signature from the Ghidra export (the evidence '
                             'gives none)' % (batch_id, r['address']))
            if sig:
                sigs[r['address']] = sig
        # A retried batch reuses the phase's results when nothing it measured
        # changed (frida-cache.json in the batch's driver folder; the key is
        # _frida_cache_key's, taken before the build).  Either way the
        # (traces, info, got) go through one tail below.
        key_hex = self._frida_cache_key(batch_id, worktree, rows, sigs, run_scenarios) \
            if (self.frida_cache or self._frida_cache_writes) else None
        cache_path = os.path.join(self.driver_dir, batch_id, 'frida-cache.json')
        cached = self._frida_cache_read(batch_id, cache_path, key_hex) \
            if key_hex and self.frida_cache else None
        info = {}
        if cached is not None:
            self.log('%s: frida: results reused from %s (key %s)' % (
                batch_id, cached['at'], key_hex[:12]))
            traces = cached['traces']
            info = {'typed_from': dict(cached.get('typed_from') or {}),
                    'typed_slots': {k: list(v) for k, v in (cached.get('typed_slots') or {}).items()}}
            got = truth.frida_verdicts(traces, rows, cached.get('skipped') or {}, info=info)
            got = {key(a): v for a, v in got.items()}
        else:
            checkout, map_path, why = self._frida_target(batch_id, worktree)
            traces = {}

            def run_frida(batch_id, table, scenarios):
                # Ours runs the batch worktree's own build (the map's RVAs).
                extra = {'run': _run_probe} if self.frida_run is truth.frida_run else {}
                traces.update(self.frida_run(batch_id, table, scenarios,
                                             build_checkout=checkout, **extra))
                return traces
            # A phase-wide failure (no build, the run raised, every scenario
            # failed on infrastructure) is an exception for the human at stage
            # frida: no row is measured, parked or rewritten (ruled).  Row-level
            # outcomes below still park.
            if why:
                raise _StageError('frida', 'the Frida phase could not run: %s; no row was '
                                           'measured, parked or rewritten; fix it and clear this '
                                           'exception' % why)
            try:
                with open(map_path, encoding='utf-8', errors='replace') as f:
                    map_text = f.read()
                self._stage(batch_id, 'frida')
                got = truth.frida_batch(batch_id, rows, sigs, map_text, scenarios=run_scenarios,
                                        run_frida=run_frida,
                                        headers=truth._our_headers(worktree), info=info)
                got = {key(a): v for a, v in got.items()}
            except Exception as e:
                raise _StageError('frida', 'the Frida run raised %s; no row was measured, parked '
                                           'or rewritten; fix it and clear this exception'
                                  % _one_line(repr(e), 300)) from None
        for sc, why in sorted((info.get('uninformative') or {}).items()):
            self.log('%s: frida: %s measured nothing (%s) and is left out' % (batch_id, sc, why))
        dead = _infrastructure_failure(traces)
        if dead:
            raise _StageError('frida', '%s; no row was measured, parked or rewritten; fix it '
                                       'and clear this exception' % dead)
        refused = _schema_failure(traces)
        if refused:
            raise _StageError('frida', 'the state dumps of a Frida run fail the replay schema '
                                       '(tooling out of step, not the rows): %s; no row was '
                                       'measured, parked or rewritten; fix it and clear this '
                                       'exception' % refused)
        if cached is None and key_hex:
            self._frida_cache_write(batch_id, cache_path, key_hex, run_scenarios, traces, info)
        # a row coverage says Frida cannot hook reads so unless the phase itself
        # measured it (equal or different: a crash recorded as different, a hook
        # the original now accepts) -- that verdict is never masked
        got.update({k: ('not measured', COVERAGE_UNHOOKABLE) for k in self._frida_unhookable
                    if got.get(k, ('',))[0] not in ('equal', 'different')})
        self._frida_got = {key(r['address']): got.get(key(r['address']), ('skipped', 'no result'))
                           for r in rows}
        # Frequency-only differences are caller findings: ledger lines in
        # _land, never a finding or a verdict (the final review's ruling).
        self._frida_notes = {key(a): list(n) for a, n in (info.get('notes') or {}).items()}
        exact = {key(a): list(v) for a, v in (info.get('exact') or {}).items()}
        return self._frida_findings(batch_id, worktree, rows, got, traces, info, exact,
                                    verdicts, importing, mb, commit)

    @staticmethod
    def _file_sha(path):
        with open(path, 'rb') as f:
            return hashlib.sha256(f.read()).hexdigest()

    def _worktree_listing(self, worktree):
        """The `git ls-tree -r` lines of the batch worktree's WORKING tree (what
        play.sync_windows copies to the build: modified and untracked files
        included, ignored ones not) with everything under docs/ left out
        (the fix rounds the cache serves edit only evidence there).  Taken
        through a temporary index so the real one is untouched.  Raises
        RuntimeError when a git step fails."""
        tmp = tempfile.mkdtemp(prefix='frida-key-')
        try:
            env = dict(os.environ, GIT_INDEX_FILE=os.path.join(tmp, 'index'))

            def git(*args):
                r = subprocess.run(['git', '-C', worktree, *args], capture_output=True,
                                   text=True, env=env)
                if r.returncode != 0:
                    raise RuntimeError('git %s: %s' % (' '.join(args[:2]),
                                                       _one_line(r.stderr or r.stdout, 120)))
                return r.stdout
            git('read-tree', 'HEAD')
            git('add', '-A')
            tree = git('write-tree').strip()
            lines = git('ls-tree', '-r', tree).splitlines()
            return [l for l in lines if not l.partition('\t')[2].startswith('docs/')]
        finally:
            shutil.rmtree(tmp, True)

    def _frida_cache_key(self, batch_id, worktree, rows, sigs, scenarios):
        """sha256 hex over what the phase measures, or None (logged: no reuse,
        no cache write) when a part of it cannot be read:
        - the batch worktree's working tree minus docs/ (_worktree_listing:
          src, compat, oracle, platform, the cmake files, third_party: the
          inputs of the x86 build, hence the map and the hook RVAs);
        - the tooling's bytes from the driver checkout (truth.ROOT):
          tools/truth.py, tools/oracle.py, tools/frida/*.py,
          oracle/replay/schema.json and screens.json, and each scenario's
          coverage.entry_sha; with --frida-map also that path, the map and
          the injector.exe and recorder.dll beside it;
        - the rows (address, name), their signatures and the sorted scenarios."""
        truth = _tool('truth')
        h = hashlib.sha256()
        try:
            h.update('\n'.join(self._worktree_listing(worktree)).encode())
            root = truth.ROOT
            names = ['tools/truth.py', 'tools/oracle.py', 'tools/minischema.py',
                     'oracle/replay/schema.json', 'oracle/replay/screens.json',
                     'oracle/replay/known_differences.json']   # the dumps verdict's inputs
            names += sorted('tools/frida/' + n for n in os.listdir(os.path.join(root, 'tools', 'frida'))
                            if n.endswith('.py')) if os.path.isdir(os.path.join(root, 'tools', 'frida')) else []
            for rel in names:
                p = os.path.join(root, rel)
                if os.path.isfile(p):
                    h.update(('\nfile %s %s' % (rel, self._file_sha(p))).encode())
            cov = _tool('coverage')
            for sc in sorted(scenarios):
                h.update(('\nscenario %s %s' % (sc, cov.entry_sha(sc, root))).encode())
            if self.frida_map:
                oracle = _tool('oracle')
                checkout = os.path.dirname(os.path.dirname(os.path.abspath(self.frida_map)))
                extra = [self.frida_map] + [os.path.join(_map_checkout(self.frida_map) or checkout,
                                                         oracle.REPLAY_DIR_REL, n)
                                            for n in (oracle.INJECTOR, oracle.RECORDER)]
                for p in extra:
                    h.update(('\nmap %s %s' % (os.path.abspath(p), self._file_sha(p))).encode())
        except (RuntimeError, OSError) as e:
            self.log('%s: frida: cache key unavailable (%s); no reuse, no cache write' % (
                batch_id, _one_line(repr(e), 160)))
            return None
        h.update(json.dumps({'rows': sorted((r['address'], r.get('name')) for r in rows),
                             'sigs': sigs, 'scenarios': sorted(scenarios)},
                            sort_keys=True, default=str).encode())
        return h.hexdigest()

    def _frida_cache_read(self, batch_id, path, key_hex):
        """The cache entry when reuse applies (same key, and every trace file
        it cites present with its recorded sha256), else None (one log line
        when the file exists but cannot be used)."""
        try:
            with open(path, encoding='utf-8') as f:
                cached = json.load(f)
        except FileNotFoundError:
            return None
        except (OSError, ValueError) as e:
            self.log('%s: frida: cache %s unreadable (%s); running the phase' % (
                batch_id, path, _one_line(repr(e), 120)))
            return None
        if not (isinstance(cached, dict) and isinstance(cached.get('traces'), dict)
                and isinstance(cached.get('at'), str) and isinstance(cached.get('files'), dict)
                and isinstance(cached.get('key'), str)):
            self.log('%s: frida: cache %s unreadable (wrong shape); running the phase' % (
                batch_id, path))
            return None
        if cached['key'] != key_hex:
            self.log('%s: frida: cache key changed (%s \u2192 %s); running' % (
                batch_id, cached['key'][:12], key_hex[:12]))
            return None
        for fpath, sha in sorted(cached['files'].items()):
            try:
                ok = self._file_sha(fpath) == sha
            except OSError:
                ok = False
            if not ok:
                self.log('%s: frida: cached trace file %s is missing or changed; running' % (
                    batch_id, fpath))
                return None
        return cached

    def _frida_cache_write(self, batch_id, path, key_hex, scenarios, traces, info):
        """Store a phase that ran and produced verdicts: key, time, scenarios,
        the traces frida_run returned (event lists included), the sha256 of
        every trace file they cite, and what frida_verdicts needs beside
        them (typed_from, typed_slots, skipped).  Not stored (logged): a phase
        in which a scenario failed, a side exited non-zero or the dumps did
        not match (a sandbox flake must not be frozen), or whose traces are
        not serialisable."""
        bad = []
        files = {}
        for sc, t in sorted(traces.items()):
            if not isinstance(t, dict):
                bad.append('%s: not a trace' % sc)
                continue
            if t.get('failed'):
                bad.append('%s failed' % sc)
            for side in ('orig', 'ours'):
                if t.get(side + '_exit'):
                    bad.append('%s %s exited %s' % (sc, side, t[side + '_exit']))
                if t.get(side + '_path'):
                    try:
                        files[t[side + '_path']] = self._file_sha(t[side + '_path'])
                    except OSError as e:
                        bad.append('%s trace file unreadable (%s)' % (sc, e.__class__.__name__))
            d = t.get('dumps')
            if isinstance(d, dict) and d.get('match') is not True:
                bad.append('%s dumps did not match' % sc)
        if bad:
            self.log('%s: frida: results not cached (%s)' % (batch_id, '; '.join(bad)[:300]))
            return
        try:
            text = json.dumps({
                'key': key_hex,
                'at': datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'),
                'scenarios': sorted(scenarios), 'traces': traces, 'files': files,
                'typed_from': info.get('typed_from') or {},
                'typed_slots': info.get('typed_slots') or {},
                'skipped': info.get('skipped') or {}}, indent=1)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            tmp = path + '.tmp'
            with open(tmp, 'w', encoding='utf-8') as f:
                f.write(text)
            os.replace(tmp, path)
        except (TypeError, ValueError, OSError) as e:
            self.log('%s: frida: results not cached (%s)' % (batch_id, _one_line(repr(e), 120)))

    def _frida_findings(self, batch_id, worktree, rows, got, traces, info, exact, verdicts,
                        importing, mb, commit=None):
        """The per-row outcomes of a Frida phase (or of a coverage skip): the
        log lines, the import findings, the trace evidence written, and the
        unmeasured WSO markings (rule 1) committed."""
        key = _tool('wave')._addr_key
        truth = _tool('truth')
        sides = {key(a): s for a, (s, _c) in verdicts.items()}
        findings, written, kept_ok, cites = [], [], [], []
        digests = {}
        for r in rows:
            k = key(r['address'])
            status, detail = got.get(k, ('skipped', 'no result'))
            self.log('%s: frida %s %s (%s)' % (batch_id, r['address'], status, detail))
            for note in self._frida_notes.get(k, ()):
                self.log('%s: frida %s %s (a caller finding, for the ledger)'
                         % (batch_id, r['address'], note))
            if importing:
                # The citation names a scenario whose compare returned 0;
                # without info (an injected frida_batch) the detail's first.
                cite = (exact.get(k) or [None])[0] if 'exact' in info else (
                    detail[len(truth.EQUAL_PREFIX):].split(', ')[0]
                    if detail.startswith(truth.EQUAL_PREFIX) else None)
                finding = self._import_outcome(batch_id, worktree, r, status, detail, traces, mb,
                                               cite)
                if finding:
                    findings.append(finding)
                elif status == 'equal':
                    written.append(r['address'])
                    digests[r['address']] = _tool('check_evidence').body_digest(
                        worktree, r['address'], r.get('file'))
                if r.get('kept'):
                    # recorded whatever the outcome: close lets it close as
                    # kept only with this batch's trace, else refuses it
                    kept_ok.append(r['address'])
                continue
            # The phase measured the batch's own build: the body on the branch
            # is the recorded side's (ours or neither keep ours', a fork verdict
            # adopted the fork's).  Equal confirms that side (measured frida,
            # _measured_for); different is a finding whatever the side.
            side = sides.get(k)
            if commit is None and os.path.isfile(self._before_path(batch_id, r['address'])):
                # a retry: the earlier phase's trace is replaced by this one's
                self._withdraw_trace(batch_id, worktree, [r['address']], 'measured again')
            if side in ('ours', 'fork') and status == 'equal':
                cite = (exact.get(k) or [None])[0] if 'exact' in info else (
                    detail[len(truth.EQUAL_PREFIX):].split(', ')[0]
                    if detail.startswith(truth.EQUAL_PREFIX) else None)
                dirty = self._dirty_source(worktree, r) if status == 'equal' else None
                if dirty:
                    self.log('%s: %s: %s has uncommitted changes at the Frida phase; trace '
                             'evidence not written' % (batch_id, r['address'], dirty))
                    self._wave_notes[k] = '%s has uncommitted changes at the Frida phase' % dirty
                    self._frida_got[k] = ('not measured', self._wave_notes[k])
                    continue
                ok, why = self._wave_trace_evidence(batch_id, worktree, r, cite, traces,
                                                    keep_before=commit is None)
                self._wave_notes[k] = why
                if ok:
                    written.append(r['address'])
                    cites.append(cite)
                # a digest for every equal row: _land withholds `measured` from a
                # body that moved since (measured_body of a row on oracle/replay
                # evidence guards nothing else)
                digests[r['address']] = _tool('check_evidence').body_digest(
                    worktree, r['address'], r.get('file'))
            if side and status == 'different':
                findings.append('%s: the Frida trace diff shows the body on the branch (verdict %s) '
                                'different from the original (%s); recheck it' % (
                                    r['address'], side, detail))
        if written and commit is not None:
            # a caller that commits the rewritten files itself (remeasure: with
            # its own files, in one commit)
            commit(written, cites, findings)
        elif written:
            self._commit_trace_evidence(batch_id, worktree, written, None if importing else (
                'frida: trace evidence for %d row(s) measured equal (%s)' % (
                    len(written), ', '.join(sorted(set(cites))))))
        if self._unmeasured_marked:
            self._commit_unmeasured_evidence(batch_id, worktree, self._unmeasured_marked)
        if (kept_ok or digests) and commit is None:
            self._record_kept(batch_id, kept_ok, digests)
        return findings

    def _clear_measured(self, batch_id):
        """Drop the phase's records (rows[].outcome, rows[].measured_body, the
        batch's measured_deps) of an earlier run of the batch."""
        with self._lock:
            state = self.load_state(self.state_path)
            batch = state['batches'].get(batch_id)
            if batch is None:
                return
            dirty = batch.pop('measured_deps', None) is not None
            for r in batch.get('rows', []):
                for f in ('outcome', 'measured_body'):
                    if f in r:
                        del r[f]
                        dirty = True
            if dirty:
                _tool('wave').save_state(self.state_path, state)

    def _section_problems(self, batch_id, worktree, report):
        """The report-section mistakes of an import batch's base-body rows
        (category theirs_changed_ours_base): a row under ## Imported whose body
        is unchanged against the merge base, a row under ## Kept whose body
        changed.  Parked rows are exempt."""
        wave = _tool('wave')
        key = wave._addr_key
        try:
            touched = {key('0x%X' % a) for _p, a, _n in
                       _tool('check_evidence').touched(worktree, self.base)}
        except Exception as e:
            self.log('%s: sections: cannot tell which functions the batch touched (%s)'
                     % (batch_id, _one_line(repr(e), 200)))
            return []
        parked, _bad = wave._parked_rows(worktree)
        batch = self.load_state(self.state_path)['batches'][batch_id]
        base_rows = {key(r['address']) for r in batch.get('rows', [])
                     if r.get('category') == 'theirs_changed_ours_base'} - set(parked)
        imported = {key(a) for a in parse_imported(report)} & base_rows
        # a row the phase recorded as kept and the worker then edited gets the one
        # line below (edit it back to the measured body); any other kept row whose
        # body changed gets the brief's two options
        edited = {k: a for k, a in kept_edited_rows(batch, worktree, key).items()
                  if k in base_rows}
        kept = {key(a) for a in self._kept} & base_rows - set(edited)
        out = ['0x%s listed under ## Imported but its body is unchanged against the merge '
               'base; list it under ## Kept (it was measured on our body) or import it'
               % k.upper() for k in sorted(imported - touched)]
        out += ['0x%s listed under ## Kept but its body changed against the merge base; put '
                'the body back to the merge base\'s or list it under ## Imported' % k.upper()
                for k in sorted(kept & touched)]
        out += ['%s: a kept row the Frida phase measured on our body; its body changed; edit it '
                'back to the measured body (it is in the frozen block); do not park it and do '
                'not move it to ## Imported' % a for _k, a in sorted(edited.items())]
        return out

    def _clear_kept_outcomes(self, batch_id, kept):
        """A row missing from the current ## Kept loses a recorded 'kept'
        outcome (a retry, or a fix round that took the row off the section)."""
        key = _tool('wave')._addr_key
        keep = {key(a) for a in kept}
        with self._lock:
            state = self.load_state(self.state_path)
            batch = state['batches'].get(batch_id)
            if batch is None:
                return
            dirty = False
            for r in batch.get('rows', []):
                if r.get('outcome') == 'kept' and key(r['address']) not in keep:
                    del r['outcome']
                    dirty = True
            if dirty:
                _tool('wave').save_state(self.state_path, state)

    def _record_kept(self, batch_id, addresses, digests=None):
        """rows[].outcome = 'kept' in the import state for the kept rows the
        phase measured (whatever came back: only a row whose trace evidence
        was written closes as kept), and rows[].measured_body (the digest of
        the body each equal row had when the phase measured it, import or
        wave 1, trace evidence written or not) for `digests`; a batch the
        state does not hold (a hand-built fixture) is left alone."""
        key = _tool('wave')._addr_key
        want = {key(a) for a in addresses}
        with self._lock:
            state = self.load_state(self.state_path)
            batch = state['batches'].get(batch_id)
            if batch is None:
                return
            have = {key(a): d for a, d in (digests or {}).items() if d}
            rowkeys = {key(r['address']) for r in batch.get('rows', [])}
            for k, d in have.items():
                if k not in rowkeys:
                    # a dependency imported with the batch: its body is held
                    # to the measured one like a row's
                    batch.setdefault('measured_deps', {})[k] = d
            for r in batch.get('rows', []):
                if key(r['address']) in want:
                    r['outcome'] = 'kept'
                if key(r['address']) in have:
                    # the body the trace measured (sha256 of its source text):
                    # the check and the close refuse a different one
                    r['measured_body'] = have[key(r['address'])]
            _tool('wave').save_state(self.state_path, state)

    def _coverage_scenarios(self, batch_id, rows):
        """(scenarios to run, skip): the phase's candidates (truth.scenarios_for)
        narrowed by the coverage map (coverage.cover_plan).  With a usable map
        the scenarios run are a greedy minimal cover of the rows some fresh
        scenario hits, plus every candidate that neither measured nor refused
        a row no fresh scenario hits (unknown); a row Frida refuses in every
        candidate is unhookable and forces nothing.  When nothing runs, skip is
        True (every row is dead or unhookable: no build, no launch).  Without
        a usable map every candidate runs."""
        truth = _tool('truth')
        cands = list(truth.scenarios_for(rows, [r.get('file') for r in rows]))
        if self.held_out:
            held = [c for c in cands if c in self.held_out]
            cands = [c for c in cands if c not in self.held_out]
            if held:
                self.log('%s: frida: held out of the phase: %s' % (batch_id, ', '.join(held)))
        self._frida_ran = (cands, [])
        self._frida_unhookable = set()
        try:
            cov_mod = _tool('coverage')
            path = os.path.join(self.repo, cov_mod.COVERAGE_REL)
            cov, why = cov_mod.check(path, self.repo)
            if cov is not None:
                # a UI scenario joins when the map says it hits a row; the
                # hold-out applies to the map's candidates as well
                cands = [c for c in cov_mod.row_scenarios(cov, rows) if c not in self.held_out]
                self._frida_ran = (cands, [])
            if cov is None:
                if os.path.exists(path):
                    self.log('%s: frida: coverage map not used (%s)' % (batch_id, why))
                self.log('%s: frida: no usable coverage map; running every scenario' % batch_id)
                return cands, False
            plan = cov_mod.cover_plan(cov, rows, cands)
        except (OSError, ValueError, KeyError, TypeError, AttributeError) as e:
            self.log('%s: frida: coverage map unreadable (%s)' % (batch_id, _one_line(repr(e), 200)))
            self.log('%s: frida: no usable coverage map; running every scenario' % batch_id)
            return cands, False
        run, left = plan['run'], plan['left']
        unhook = plan['unhookable']
        self._frida_unhookable = {_tool('wave')._addr_key(a) for a in unhook}
        if not run:
            self.log('%s: frida: skipped, coverage says no scenario exercises any row' % batch_id)
            self._frida_consulted = list(cands)
            if unhook:
                self._log_unhookable(batch_id, unhook)
            return [], True
        self._frida_ran = (list(run), left)
        self.log('%s: frida: scenarios by coverage: %s (cover %d of %d rows; left out: %s)'
                 % (batch_id, ', '.join(run), plan['covered'], plan['total'],
                    ', '.join(left) or 'none'))
        if unhook:
            self._log_unhookable(batch_id, unhook)
        return run, False

    def _log_unhookable(self, batch_id, unhook):
        self.log('%s: frida: %d row(s) Frida refuses in the original: %s'
                 % (batch_id, len(unhook), ', '.join(unhook)))

    def _import_rows(self, batch_id, batch, worktree, mb_sha):
        """The functions an import batch's Frida phase measures: its rows
        that are not parked (a parked row whose body the batch changed
        against the merge base is unparked first, Task 63: the batch
        imported it, so its body cannot go back), plus every function the
        batch touched against the merge base that carries a `// 0xADDR`
        marker (a stub dependency imported with a row: it is hooked and held
        to the same bar), less those whose evidence stands as written
        (wave.kept_evidence).  For a kept row a frida trace another batch
        wrote does not stand (wave.other_batch_trace): it measured that
        batch's body."""
        wave = _tool('wave')
        key = wave._addr_key
        parked, bad = wave._parked_rows(worktree)
        if bad:
            self.log('%s: frida: parked rows: %s' % (batch_id, bad))
        kept = {key(a) for a in (getattr(self, '_kept', None) or {})}
        try:
            touched = _tool('check_evidence').touched(worktree, self.base)
        except Exception as e:
            touched = []
            self.log('%s: frida: cannot tell which functions the batch touched (%s)'
                     % (batch_id, _one_line(repr(e), 200)))
        moved = {key('0x%X' % addr) for _p, addr, _n in touched}
        # A parked row the batch changed against the merge base is imported,
        # not parked (Task 63): unparked here, then measured or marked below
        # like the batch's own rows.
        parked, cats = self._unpark_imported(batch_id, batch, worktree, parked, moved)
        # A kept row is a base-body row whose body is untouched against the
        # merge base, or an only-theirs row ours already carries a body for
        # (our src labels the address, Task 40); any other row on the list is
        # measured as an import, with no outcome (the mechanical check
        # already tells the worker).
        rows = [dict(r) for r in batch.get('rows', []) if key(r['address']) not in parked]
        seen = {key(r['address']) for r in batch.get('rows', [])} | set(parked)
        for r in rows:
            k = key(r['address'])
            if (k in kept and k not in moved
                    and (r.get('category') == 'theirs_changed_ours_base'
                         or (r.get('category') == 'only_theirs'
                             and wave._labelled_at(worktree, mb_sha, r['address'])))):
                r['kept'] = True
        prior = self._prior_categories() if touched else {}
        for path, addr, name in touched:
            k = key('0x%X' % addr)
            if k not in seen:
                seen.add(k)
                row = {'address': '0x%X' % addr, 'name': name, 'file': path,
                       'dependency': True}
                cat = cats.get(k) or prior.get(k)
                if cat:
                    # the category the row was parked under: a stub or
                    # only-theirs row can be marked unmeasured, not measured
                    # (a row unparked before this phase keeps the category
                    # its batch was cut with)
                    row['category'] = cat
                rows.append(row)
                self.log('%s: frida: 0x%X %s imported with the batch; measured like a row'
                         % (batch_id, addr, name))
        out = []
        for r in rows:
            try:
                with open(os.path.join(worktree, wave._evidence_rel(r['address'])),
                          encoding='utf-8') as f:
                    text = f.read()
            except OSError:
                text = None
            # evidence that stands as written (an oracle case, a fork note, a
            # file unchanged since the merge base) is not measured or rewritten,
            # a kept row's included: it closes on that evidence
            why = text is not None and wave.kept_evidence(
                worktree, mb_sha, r['address'], r.get('name', ''), text)
            if why and wave.other_batch_trace(text, batch_id):
                # unchanged since the merge base, but another batch's frida
                # trace: it measured the body that batch had, and nothing ties
                # it to the one here, so a kept row, a touched row or a
                # dependency is measured again
                self.log('%s: frida %s %s on another batch\'s frida trace (%s); measured again'
                         % (batch_id, r['address'], 'kept' if r.get('kept') else 'stands',
                            wave._frida_probe(text)))
                why = None
            if why:
                if why.startswith('oracle case '):
                    # Task 39: the case is the row's measurement, so the
                    # phase neither hooks it nor parks it
                    self.log('%s: frida %s: oracle-backed; the case is its '
                             'measurement' % (batch_id, r['address']))
                else:
                    self.log('%s: frida %s kept as written (%s); not measured'
                             % (batch_id, r['address'], why))
                if r.get('kept'):
                    self._record_kept(batch_id, [r['address']])
            else:
                out.append(r)
        return out

    def _prior_categories(self):
        """The category each address was cut with in any batch of the state
        (its rows or its unparked list).  A row the batch imported that an
        earlier batch parked and someone unparked before this phase (wave.py
        unpark) keeps that category, so rule 1 can mark it unmeasured."""
        key = _tool('wave')._addr_key
        out = {}
        try:
            state = self.load_state(self.state_path)
        except Exception:
            return out
        for b in (state.get('batches') or {}).values():
            for e in list(b.get('rows') or []) + list(b.get('unparked') or []):
                if e.get('address') and e.get('category'):
                    out.setdefault(key(e['address']), e['category'])
        return out

    def _unpark_imported(self, batch_id, batch, worktree, parked, moved):
        """The parked rows this batch imported anyway (Task 63): every parked
        row whose body the batch changed against its merge base (the `moved`
        set the close check uses) is unparked the way `wave.py unpark` does
        it -- the parked file goes, the row moves to the old batch's
        `unparked` list with reason `imported by <batch>` -- because its body
        cannot go back to the merge base's (it does not exist there) and the
        close check will want a trace or the unmeasured marking for it, which
        only this phase writes.  Returns (parked, categories): the parked map
        less the unparked rows, and each unparked row's own category (a stub
        or only-theirs row can be marked unmeasured rather than measured).
        A row of this batch is not unparked here -- wave.unpark refuses the
        rows of an assigned batch and is all-or-nothing, so one refusal
        would undo every unpark -- and on any refusal nothing is unparked:
        the rows stay parked, as before."""
        wave = _tool('wave')
        key = wave._addr_key
        own = {key(r['address']) for r in batch.get('rows', [])}
        want = sorted(k for k in parked if k in moved and k not in own)
        if not want:
            return parked, {}
        folder = os.path.join(worktree, wave.IMPORT_PARKED_DIR)
        rels = {}
        try:
            names = sorted(os.listdir(folder))
        except OSError:
            names = []
        for name in names:
            # the file's own spelling is the one to commit the removal of
            if name.lower().endswith('.json') and wave.parked_stem_key(name) in want:
                rels[wave.parked_stem_key(name)] = '%s/%s' % (wave.IMPORT_PARKED_DIR, name)
        with self._lock:
            rc, text = wave.unpark(['0x' + k.upper() for k in want],
                                   'imported by %s' % batch_id,
                                   state_path=self.state_path, repo=worktree)
        for line in text.splitlines():
            if line:
                self.log('%s: %s' % (batch_id, line))
        if rc != 0:
            # a refusal (nothing written) or a parked file that would not go:
            # whatever the worktree still reads as parked stays parked
            self.log('%s: frida: the parked rows stay parked' % batch_id)
            return wave._parked_rows(worktree)[0], {}
        cats = {}
        for b in (self.load_state(self.state_path).get('batches') or {}).values():
            # the moved row carries its own category into the unparked list
            for e in b.get('unparked') or []:
                k = key(e.get('address', ''))
                if k in want and e.get('category'):
                    cats.setdefault(k, e['category'])
        if rels:
            self._commit_parked_removals(batch_id, worktree, sorted(rels.values()))
        return ({k: v for k, v in parked.items() if k not in want}, cats)

    def _commit_parked_removals(self, batch_id, worktree, rels):
        """Commit the parked-row files the phase removed (Task 63's
        auto-unpark) on the batch branch: the same plain add+commit as the
        unmeasured marking, over deletions.  The state's own move is
        committed at close, with the rest of the state."""
        def git(*args):
            return subprocess.run(['git', '-C', worktree, *args],
                                  capture_output=True, text=True)
        if not git('status', '--porcelain', '--', *rels).stdout.strip():
            return
        git('add', '--', *rels)
        r = git('commit', '-q', '-m',
                'frida: unpark %d row(s) imported by %s' % (len(rels), batch_id),
                '--', *rels)
        if r.returncode != 0:
            self.log('%s: unparked files not committed: %s' % (
                batch_id, _one_line((r.stderr or '') + (r.stdout or ''))))

    def _import_outcome(self, batch_id, worktree, row, status, detail, traces, mb, cite):
        """None when an import row measured equal and its evidence is now
        kind: trace (citing `cite`, a scenario whose compare returned 0), or
        when a stub or only-theirs row was exercised by no scenario (rule 1
        of the 2026-10-26 ruling: ours never had the body, so the import
        needs no coverage; its evidence is marked unmeasured WSO import);
        else the finding that parks it (the phase runs once per batch, so
        nothing else can measure it)."""
        a = row['address']
        kept = bool(row.get('kept'))
        ot = row.get('category') == 'only_theirs'
        def park(needs):
            return park_text(a, mb, needs, edit_back=not kept, kept=kept, only_theirs=ot)
        dep = (' It is a dependency you imported with the batch: park the rows that need '
               'it with it.' if row.get('dependency') else '')
        if status == 'equal' and cite is None:
            if kept:
                run = self._ran_scenarios()
                return ('%s: ours is equal to the original only up to a frequency difference '
                        '(%s); no scenario compared exactly, so no trace evidence can cite '
                        'one, and the phase does not run again in this batch, so park it: %s'
                        % (a, detail, park(kept_needs(run, 'frequency difference only: %s'
                                                                  % detail)))) + dep
            # Equal only as a prefix (frequency-only): no run compared exactly,
            # and a compare=0 citation of a run that did not would be false.
            return ('%s: equal only up to a frequency difference (%s); no scenario compared '
                    'exactly, so no trace evidence can cite one, and the phase does not run '
                    'again in this batch, so park it: %s' % (a, detail, park(
                        'frida frequency only, no exact compare: %s' % detail))) + dep
        if status == 'equal':
            err = self._trace_evidence(batch_id, worktree, row, cite, traces, mb, kept)
            return err + dep if err else None
        if status == 'different' and kept:
            shown = re.findall(r'(?:^differs in |; )([A-Za-z0-9_]+): ', detail) \
                or self._ran_scenarios()
            text = ('%s: our body differs from the original (%s); the base body was never '
                    'changed, so edit nothing back, and the phase does not run again in this '
                    'batch, so an import of his body cannot be measured: park it: %s'
                    % (a, detail, park('ours differs from the exe (%s): %s; import '
                                            'his body in a later batch'
                                            % (', '.join(shown), detail))))
        elif status == 'different':
            text = ('%s: the Frida trace diff shows the imported body different from the '
                    'original (%s); the phase does not run again in this batch, so park it: %s'
                    % (a, detail, park('frida different: %s' % detail)))
        elif status == 'not-exercised':
            run, left = getattr(self, '_frida_ran', None) or (
                list(_tool('truth').FRIDA_SCENARIOS), [])
            needs = ('not-exercised (%s)' % detail if detail.startswith(COVERAGE_NOT_HIT)
                     else unmeasured_needs(run, left))
            if kept:
                needs = kept_needs(run)
            if not kept and row.get('category') in _tool('wave').RULE1_CATEGORIES:
                # Rule 1 (2026-10-26 ruling): ours never had this body, so the
                # import no longer needs coverage.  Mark the evidence and let
                # the row close unmeasured (wave.py unmeasured-imports lists
                # it); only a marking that fails falls back to parking it.
                rel = _tool('wave')._evidence_rel(a)
                try:
                    _write_unmeasured_evidence(os.path.join(worktree, rel), needs)
                except (OSError, ValueError) as e:
                    self.log('%s: frida %s not-exercised: marking its evidence '
                             'failed (%s); parking it' % (batch_id, a, _one_line(str(e), 200)))
                else:
                    self._unmeasured_marked[a] = rel
                    self.log('%s: frida %s not-exercised: rule 1, imports '
                             'unmeasured (evidence marked %s)'
                             % (batch_id, a, _tool('wave').UNMEASURED_WSO))
                    return None
            text = unmeasured(a, run, park(needs))
        elif status == 'skipped':
            text = '%s: no Frida hook (%s); park it: %s' % (
                a, detail, park(kept_needs(self._ran_scenarios(), 'no Frida hook: %s' % detail)
                                if kept else 'no Frida hook: %s' % detail))
        elif status == 'not measured' and detail == COVERAGE_UNHOOKABLE:
            text = ('%s: not measured (%s); the phase does not run again in this batch, so '
                    'park it: %s' % (a, detail, park(
                        kept_needs(self._ran_scenarios(), detail) if kept
                        else 'not measured (%s)' % detail)))
        else:
            # scenario failed, not measured: nothing measured the row, and the
            # close would refuse it with no retry left.
            needs = _one_line('%s: %s' % (status, detail), 300)
            if kept:
                needs = kept_needs(self._ran_scenarios(), _one_line(
                    detail if status == 'not measured' else '%s: %s' % (status, detail), 200))
            text = ('%s: the Frida phase gave "%s" (%s); the phase does not run again in this '
                    'batch, so park it: %s' % (a, status, detail, park(needs)))
        return text + dep

    _frida_cache_writes = True     # remeasure turns the cache off (nothing reads it)
    _reuse_target = None           # remeasure: {worktree: (checkout, map, None)}, one build per pass

    def _frida_target(self, batch_id, worktree):
        if self._reuse_target is not None and worktree in self._reuse_target:
            return self._reuse_target[worktree]
        got = self._frida_target_build(batch_id, worktree)
        if self._reuse_target is not None and not got[2]:
            self._reuse_target[worktree] = got
        return got

    def _frida_target_build(self, batch_id, worktree):
        """(checkout, map path, phase error or None): what the Frida phase
        measures.  By default the batch worktree is built for x86 in its own
        Windows checkout (self.frida_build), so the bodies under test are the
        batch's and the map's RVAs are theirs; --frida-map overrides that
        with an existing build (the exe beside the map), for hand runs.  A
        failed build is returned as the phase error (the caller makes it the
        batch's exception at stage frida), never a crash."""
        if self.frida_map:
            checkout = _map_checkout(self.frida_map)
            if not os.path.isfile(self.frida_map):
                return None, None, '--frida-map %s does not exist' % self.frida_map
            if checkout is None:
                return None, None, ('--frida-map %s is not where an x86 build writes its map '
                                    '(<checkout>/%s)' % (self.frida_map, X86_MAP_REL))
            oracle = _tool('oracle')
            for name in (oracle.INJECTOR, oracle.RECORDER):
                path = os.path.join(checkout, oracle.REPLAY_DIR_REL, name)
                if not os.path.isfile(path):
                    return None, None, ('--frida-map %s: no %s beside it; the original side runs '
                                        'that checkout\'s injector and recorder (tools/oracle.py '
                                        'build from it)' % (self.frida_map, path))
            self.log('%s: frida: --frida-map given: measuring the build in %s, not a build '
                     'of %s' % (batch_id, checkout, worktree))
            return checkout, self.frida_map, None
        self._stage(batch_id, 'frida build')
        log_path = os.path.join(self.driver_dir, batch_id, 'frida-build.log')
        try:
            os.makedirs(os.path.dirname(log_path), exist_ok=True)
            checkout, map_path = self.frida_build(worktree, log_path=log_path)
        except (Exception, SystemExit) as e:
            return None, None, 'the x86 build of %s failed: %s (log %s)' % (
                worktree, _one_line(str(e) or repr(e), 300), log_path)
        self.log('%s: frida: x86 build of %s in %s' % (batch_id, worktree, checkout))
        return checkout, map_path, None

    def _evidence_kind(self, worktree, address):
        """The `kind:` of a row's evidence file in the worktree, or None."""
        wave = _tool('wave')
        try:
            with open(os.path.join(worktree, wave._evidence_rel(address)),
                      encoding='utf-8') as f:
                m = wave.KIND_RE.search(f.read())
        except OSError:
            return None
        return m.group(1) if m else None

    def _ran_scenarios(self):
        return (getattr(self, '_frida_ran', None) or (
            list(_tool('truth').FRIDA_SCENARIOS), []))[0]

    @staticmethod
    def _trace_pair(traces, scenario):
        """(orig path, ours path, dumps, why) of `scenario`'s run: why is
        'dumps' when its two sides were not shown to keep the same state (the
        dump guard), 'files' when a trace file is missing, else None."""
        t = traces.get(scenario)
        t = t if isinstance(t, dict) else {}
        orig, ours = t.get('orig_path'), t.get('ours_path')
        dumps = t.get('dumps')
        if not (isinstance(dumps, dict) and dumps.get('match') is True):
            return orig, ours, dumps, 'dumps'
        if not (orig and ours and os.path.isfile(orig) and os.path.isfile(ours)):
            return orig, ours, dumps, 'files'
        return orig, ours, dumps, None

    def _dirty_source(self, worktree, row):
        """The source file (or src/) of a row that has uncommitted changes in
        the worktree, else None.  The x86 build syncs the files on disk, the
        digest reads HEAD: a dirty file may have measured another body."""
        path = row.get('file') or 'src'
        r = subprocess.run(['git', '-C', worktree, 'status', '--porcelain', '--', path],
                           capture_output=True, text=True)
        if r.returncode != 0:
            self.log('git status of %s failed (%s); counted as uncommitted' % (
                path, _one_line(r.stderr or '', 120)))
        # fails closed: a git error counts as dirty
        return path if r.returncode != 0 or r.stdout.strip() else None

    def _before_path(self, batch_id, address):
        """driver/<batch>/evidence-before/<ADDR>.md: the evidence file as it was
        before the wave-1 phase rewrote it to kind: trace.  It exists exactly
        while the row's evidence is that driver-written trace."""
        return os.path.join(self.driver_dir, batch_id, 'evidence-before',
                            _tool('wave')._addr_key(address).upper() + '.md')

    def _before_kind(self, batch_id, address):
        """The `kind:` the evidence had before the phase rewrote it, or None."""
        try:
            with open(self._before_path(batch_id, address), encoding='utf-8') as f:
                m = _tool('wave').KIND_RE.search(f.read())
        except OSError:
            return None
        return m.group(1) if m else None

    def _wave_trace_evidence(self, batch_id, worktree, row, cite, traces, keep_before=True):
        """Wave 1: an equal row whose verdict confirms the body on the branch
        and whose evidence is still kind: read-only is rewritten to kind:
        trace (the import path's _write_trace_evidence, the same dump guard;
        the exemption text is kept).  The file as it was goes to
        _before_path first (unless keep_before is False: remeasure).  (True, None) when the file was rewritten, else
        (False, why): a refusal is logged, never a finding (the verdict stands
        as measured frida either way)."""
        wave = _tool('wave')
        a = row['address']
        kind = self._evidence_kind(worktree, a)
        if kind != 'read-only':
            return False, 'evidence is kind %s; it stands' % (kind or 'missing')
        if cite is None:
            return False, ('equal only up to a frequency difference; no scenario compared '
                           'exactly, so no trace evidence can cite one')
        orig, ours, dumps, bad = self._trace_pair(traces, cite)
        if bad:
            why = ('state dumps not shown to match: %s' % ((dumps or {}).get('detail')
                                                           or 'not compared') if bad == 'dumps'
                   else 'trace files missing')
            self.log('%s: frida %s equal in %s but no trace evidence written (%s)' % (
                batch_id, a, cite, why))
            return False, why
        path = os.path.join(worktree, wave._evidence_rel(a))
        before = self._before_path(batch_id, a)
        try:
            if keep_before:
                os.makedirs(os.path.dirname(before), exist_ok=True)
                shutil.copyfile(path, before)
            _write_trace_evidence(path, '%s-%s' % (batch_id, cite), orig, ours,
                                  keep_exemption=True)
        except (OSError, ValueError) as e:
            self.log('%s: frida %s: evidence not rewritten to kind: trace (%s)' % (batch_id, a, e))
            try:
                os.unlink(before)
            except OSError:
                pass
            return False, 'evidence not rewritable: %s' % e
        return True, None

    def _withdraw_trace(self, batch_id, worktree, addresses, why):
        """Put the pre-rewrite evidence back for each address (from
        _before_path), commit it, drop the digest the state recorded and the
        line the status kept."""
        wave = _tool('wave')
        key = wave._addr_key
        rels, gone = [], []
        for a in addresses:
            before = self._before_path(batch_id, a)
            rel = wave._evidence_rel(a)
            try:
                with open(before, encoding='utf-8', newline='') as f:
                    saved = f.read()
                if not _undo_trace_evidence(os.path.join(worktree, rel), saved, batch_id):
                    self.log('%s: %s: evidence front matter unreadable; the pre-rewrite copy '
                             'replaced the file' % (batch_id, a))
                os.unlink(before)
            except OSError as e:
                self.log('%s: %s: trace evidence not withdrawn (%s)' % (batch_id, a, e))
                continue
            rels.append(rel)
            gone.append(a)
        if not rels:
            return []
        def drop(s):
            written = (s['batches'].get(batch_id) or {}).get('frida_written') or {}
            for a in gone:
                written.pop(key(a), None)
        self._update_status(drop)
        with self._lock:
            state = self.load_state(self.state_path)
            batch = state['batches'].get(batch_id)
            if batch is not None:
                for r in batch.get('rows', []):
                    if key(r['address']) in {key(a) for a in gone}:
                        r.pop('measured_body', None)
                wave.save_state(self.state_path, state)
        self._git_in(worktree, 'add', '--', *rels)
        r = self._git_in(worktree, 'commit', '-q', '-m',
                         'frida: trace evidence withdrawn for %d row(s) of %s (%s)' % (
                             len(rels), batch_id, why), '--', *rels)
        if r.returncode != 0:
            self.log('%s: trace evidence withdrawal not committed: %s' % (
                batch_id, _one_line((r.stderr or '') + (r.stdout or ''))))
        return gone

    def _body_moved(self, batch_id, worktree, address):
        """True when the phase recorded the digest of a row's body and the
        body on the branch is another one now."""
        key = _tool('wave')._addr_key
        batch = self.load_state(self.state_path)['batches'].get(batch_id) or {}
        row = next((r for r in batch.get('rows', []) if key(r['address']) == key(address)), None)
        want = row.get('measured_body') if row else None
        return bool(want) and _tool('check_evidence').body_digest(
            worktree, address, row.get('file')) != want

    def _withdraw_changed(self, batch_id, worktree):
        """A wave-1 row the phase wrote trace evidence for (its body digest is
        in the state) whose body on the branch is another one now: the trace
        measured a body that does not land, so the evidence goes back to what
        it was and the row is not measured."""
        key = _tool('wave')._addr_key
        ce = _tool('check_evidence')
        batch = self.load_state(self.state_path)['batches'].get(batch_id) or {}
        stale = [r['address'] for r in batch.get('rows', [])
                 if r.get('measured_body') and os.path.isfile(self._before_path(batch_id, r['address']))
                 and ce.body_digest(worktree, r['address'], r.get('file')) != r['measured_body']]
        for a in self._withdraw_trace(batch_id, worktree, stale, 'body changed after the Frida phase'):
            self.log('%s: %s: body changed after the Frida phase; trace evidence withdrawn' % (batch_id, a))
            self._frida_got[key(a)] = ('not measured', 'body changed after the Frida phase')

    def _trace_evidence(self, batch_id, worktree, row, scenario, traces, mb, kept=False):
        """Rewrite one equal import row's evidence to kind: trace, citing
        `scenario`'s run (one whose compare returned 0); None, or the finding
        that says why it could not be."""
        wave = _tool('wave')
        ot = row.get('category') == 'only_theirs'
        orig, ours, dumps, bad = self._trace_pair(traces, scenario)
        if bad == 'dumps':
            # The dump guard: trace evidence only from a run whose two sides
            # kept the same state (frida_run compares their dumps).
            return ('%s: measured equal in %s but that run\'s state dumps were not shown to '
                    'match (%s); trace evidence comes only from a run whose dumps match and '
                    'the phase does not run again in this batch, so park it: %s' % (
                        row['address'], scenario, (dumps or {}).get('detail') or 'not compared',
                        park_text(row['address'], mb, kept_needs(
                            scenario, 'dumps not shown to match') if kept
                            else 'not measured: dumps not shown to match in %s' % scenario,
                            edit_back=not kept, kept=kept, only_theirs=ot)))
        if bad == 'files':
            return ('%s: measured equal in %s but its trace files are missing; the phase does '
                    'not run again in this batch, so park it: %s' % (
                        row['address'], scenario, park_text(
                            row['address'], mb, kept_needs(scenario, 'trace files missing')
                            if kept else 'frida trace files missing', edit_back=not kept, kept=kept, only_theirs=ot)))
        dirty = self._dirty_source(worktree, row)
        if dirty:
            self.log('%s: %s: %s has uncommitted changes at the Frida phase; trace evidence '
                     'not written' % (batch_id, row['address'], dirty))
            return ('%s: measured equal in %s but %s has uncommitted changes (the measured body '
                    'is not the committed one); the phase does not run again in this batch, so '
                    'park it: %s' % (row['address'], scenario, dirty, park_text(
                        row['address'], mb, kept_needs(scenario, 'source file uncommitted')
                        if kept else 'source file has uncommitted changes at the Frida phase',
                        edit_back=not kept, kept=kept, only_theirs=ot)))
        path = os.path.join(worktree, wave._evidence_rel(row['address']))
        try:
            _write_trace_evidence(path, '%s-%s' % (batch_id, scenario), orig, ours)
        except (OSError, ValueError) as e:
            return ('%s: its evidence could not be rewritten to kind: trace (%s); the phase does '
                    'not run again in this batch, so park it: %s' % (
                        row['address'], e, park_text(
                            row['address'], mb, kept_needs(
                                scenario, 'evidence not rewritable: %s' % e) if kept
                            else 'evidence not rewritable: %s' % e, edit_back=not kept, kept=kept, only_theirs=ot)))
        return None

    def _frida_lines(self, batch_id):
        """{address key: trace_source} of the frida lines this batch's phase
        wrote (kept in the status file across a park), for the check that a
        worker wrote none."""
        saved = (self._read_status()['batches'].get(batch_id) or {}).get('frida_written')
        return dict(saved) if isinstance(saved, dict) else {}

    def _commit_trace_evidence(self, batch_id, worktree, addresses, message=None):
        wave = _tool('wave')
        ce = _tool('check_evidence')
        rels = [wave._evidence_rel(a) for a in addresses]
        lines = {}
        for a, rel in zip(addresses, rels):
            try:
                with open(os.path.join(worktree, rel), encoding='utf-8') as f:
                    lines[wave._addr_key(a)] = ce.front_matter(f.read()).get('trace_source')
            except OSError:
                pass
        self._update_status(lambda s: s['batches'].setdefault(batch_id, {}).setdefault(
            'frida_written', {}).update(lines))

        def git(*args):
            return subprocess.run(['git', '-C', worktree, *args],
                                  capture_output=True, text=True)
        if not git('status', '--porcelain', '--', *rels).stdout.strip():
            return
        git('add', '--', *rels)
        r = git('commit', '-q', '-m', message or 'frida: trace evidence for %d rows of %s' % (
            len(rels), batch_id), '--', *rels)
        if r.returncode != 0:
            self.log('%s: trace evidence not committed: %s' % (
                batch_id, _one_line((r.stderr or '') + (r.stdout or ''))))

    def _commit_unmeasured_evidence(self, batch_id, worktree, marked):
        """Commit the evidence files the phase marked as unmeasured WSO
        imports (rule 1 of the 2026-10-26 ruling): a plain git add+commit.
        Unlike the trace evidence this never touches the status file's
        frida_written -- there is no trace_source to remember, and the
        marking stands on its own (wave.unmeasured-imports reads it)."""
        rels = sorted(set(marked.values()))

        def git(*args):
            return subprocess.run(['git', '-C', worktree, *args],
                                  capture_output=True, text=True)
        if not git('status', '--porcelain', '--', *rels).stdout.strip():
            return
        git('add', '--', *rels)
        r = git('commit', '-q', '-m',
                'frida: unmeasured WSO import marking for %d row(s) of %s'
                % (len(marked), batch_id), '--', *rels)
        if r.returncode != 0:
            self.log('%s: unmeasured WSO marking not committed: %s' % (
                batch_id, _one_line((r.stderr or '') + (r.stdout or ''))))

    # -- the probe queue (Task 28) -----------------------------------------------

    PROBE_STATE_SIBLING = 'wave-state.json'

    @property
    def wave_state_path(self):
        """The wave-1 state file the probe queue resolves rows into (beside the
        probe state)."""
        return os.path.join(os.path.dirname(os.path.abspath(self.state_path)),
                            self.PROBE_STATE_SIBLING)

    def _probe_listed(self):
        """{address key: entry} of docs/wso/probe-list.json."""
        wave = _tool('wave')
        entries, err = wave._load_list(self.probe_path, 'probe list', missing_ok=True)
        if err:
            raise ValueError(err.strip())
        return {wave._addr_key(e['address']): e for e in entries}

    def _probe_list_edit(self, address, reason=None, append=None, stamp=None):
        """Refresh (`reason`) or extend (`append`, once) the reason of a probe
        list entry, and stamp `probed` (batch, scenarios that ran, `hits` the map
        gave the row, pin, `hand` for a row only a pin move re-offers) so `next`
        offers the row again only for a new hitting scenario or a moved pin;
        False when the row is not on the list."""
        wave = _tool('wave')
        with self._lock:
            entries, err = wave._load_list(self.probe_path, 'probe list', missing_ok=True)
            if err:
                raise ValueError(err.strip())
            for e in entries:
                if wave._addr_key(e['address']) != wave._addr_key(address):
                    continue
                if reason is not None:
                    e['reason'] = reason
                elif append and append not in e.get('reason', ''):
                    e['reason'] = (e.get('reason', '').rstrip() + '; ' + append).lstrip('; ')
                if stamp is not None:
                    e['probed'] = stamp
                wave._save_json(self.probe_path, entries)
                return True
        return False

    def _probe_outcomes(self, batch_id, outcomes):
        """rows[].outcome (and measured_body / case) of the probe state:
        {address: {field: value}}; a field set to None is dropped."""
        key = _tool('wave')._addr_key
        want = {key(a): f for a, f in outcomes.items()}
        with self._lock:
            state = self.load_state(self.state_path)
            batch = state['batches'].get(batch_id)
            if batch is None:
                return
            for r in batch.get('rows', []):
                for f, v in (want.get(key(r['address'])) or {}).items():
                    if v is None:
                        r.pop(f, None)
                    else:
                        r[f] = v
            _tool('wave').save_state(self.state_path, state)

    def _probed_stamp(self, batch_id, row, batch, scenarios, hand=False):
        """The `probed` stamp of a probe-list entry the phase refreshed: the
        batch, the scenarios that ran for it, the scenarios the coverage map
        says hit it (`hits`: what `next` compares a later map with), the pin its
        body came from, and `hand` for a row only a person can rule on."""
        stamp = {'batch': batch_id, 'scenarios': sorted(scenarios or ()),
                 'hits': _tool('wave').probe_hits(row, self.disagreements_path, None,
                                                  self.repo, self.repo),
                 'pin': row.get('fork_base') or batch.get('fork_base')
                 or _tool('wave').DEFAULT_FORK_BASE}
        if hand:
            stamp['hand'] = True
        return stamp

    def _record_probe_rows(self, batch_id, worktree):
        """The record stage of a probe batch: every `fork-equal` row (decided by
        the Frida phase, checks and review passed) goes through resolve_probe
        (fork, measured frida, the losing kind the phase saw) and becomes
        `resolved`.  A body that moved since the phase measured it is a
        failure; a row already off the probe list with a wave-state verdict on
        its case (a retry of this stage) is only marked.  Returns the failures."""
        wave = _tool('wave')
        ce = _tool('check_evidence')
        key = wave._addr_key
        failed = []
        batch = self.load_state(self.state_path)['batches'][batch_id]
        for r in batch.get('rows', []):
            if r.get('outcome') != 'fork-equal':
                continue
            a, k = r['address'], key(r['address'])
            if r.get('measured_body') and ce.body_digest(
                    worktree, a, r.get('file')) != r['measured_body']:
                failed.append('%s: body changed after the Frida phase; not recorded' % a)
                continue
            with self._lock:
                on_list = k in self._probe_listed()
                wave_state = self.load_state(self.wave_state_path)
                done = any(v.get('side') == 'fork' and v.get('case') == r.get('case')
                           for b in wave_state['batches'].values()
                           for ad, v in (b.get('verdicts') or {}).items() if key(ad) == k)
                if on_list or not done:
                    rc, text = wave.resolve_probe(
                        a, 'fork', r['case'], state_path=self.wave_state_path, repo=self.repo,
                        measured='frida', losing_kind=r.get('losing_kind') or 'read-only',
                        evidence_root=worktree)
                    if rc != 0:
                        failed.append('%s: resolve_probe failed: %s' % (a, text.strip()))
                        continue
                    self.log('%s: %s' % (batch_id, text.rstrip()))
            self._probe_outcomes(batch_id, {a: {'outcome': 'resolved'}})
        return failed

    def _probe_gap(self, batch_id, row, scenario, detail):
        """An engine-gaps.json entry (the shape remeasure writes) in the repo
        the driver commits with the state; one per row and batch."""
        wave = _tool('wave')
        with self._lock:
            path, gaps = self._remeasure_json_list(self.repo, self.ENGINE_GAPS_REL)
            if any(g.get('address') == row['address'] and g.get('found_by') == batch_id
                   for g in gaps if isinstance(g, dict)):
                return
            gaps.append({'address': row['address'], 'name': row.get('name', ''),
                         'scenario': scenario, 'detail': detail, 'found_by': batch_id,
                         'at': _now()})
            wave._save_json(path, gaps)

    def _deciding_sentence(self, worktree, address, pin, case, scenario):
        """Append `disagreed with WSO at <pin>: ...; case <case> decides` under
        the row's `## WSO delta` when the file lacks it (the worker wrote the
        delta paragraph and never the case)."""
        wave = _tool('wave')
        path = os.path.join(worktree, wave._evidence_rel(address))
        with open(path, encoding='utf-8', newline='') as f:
            text = f.read()
        tail = 'case %s decides' % case
        if any(l.rstrip().rstrip('.').endswith(tail) for l in text.splitlines()):
            return False
        line = ('disagreed with WSO at %s: ours differed from the fork body; the fork body, '
                'built into this branch, behaves like the exe in the Frida trace of %s; '
                'case %s decides' % (pin, scenario, case))
        eol = '\r\n' if '\r\n' in text else '\n'
        new = wave._under_heading(text.replace('\r\n', '\n'), wave.WSO_DELTA_HEAD, line)
        with open(path, 'w', encoding='utf-8', newline='') as f:
            f.write(new.replace('\n', eol) if eol != '\n' else new)
        return True

    def _probe_back_text(self, worktree, address, why):
        """The fix-round problem of a row that stays on the list while the
        branch still carries the adopted body."""
        wave = _tool('wave')
        mb_sha = wave.merge_base_sha(worktree, self.base)
        mb = mb_sha[:12] if mb_sha else self.base
        return ('%s: %s; the row stays on the probe list: %s and commit; put '
                '%s back as the merge base has it (`git show %s:%s` shows it; delete it when '
                'the merge base has none) and commit; %s.'
                % (address, why, EDIT_BACK % mb, wave._evidence_rel(address), mb,
                   wave._evidence_rel(address), SHARED_FILE))

    def _probe_phase(self, batch_id, batch, worktree):
        """After the worker of a probe batch: the Frida phase over the rows whose
        fork body the worker adopted (`candidate <addr> fork`, body changed
        against the merge base, still on the probe list), then per row:

        - equal (the dump guard held, trace evidence written): the evidence
          is rewritten to kind: trace by the same writer wave 1 uses (probe
          `<batch>-<scenario>`), the deciding sentence `...; case
          frida-<batch>-<scenario> decides` is added under `## WSO delta`
          when missing, committed, and the row's probe-state record becomes
          outcome `fork-equal` (case, measured_body, losing_kind from the
          evidence the merge base had).  Nothing is recorded in the wave state
          or on the probe list here: the record stage of `_land` (after the
          checks and the review) calls resolve_probe;
        - different: the fork body is wrong too: the finding is appended to
          the entry's reason, an engine-gaps.json entry written, the body to
          go back to ours (a problem for the fix round);
        - anything else (not exercised, no hook, not measured): the entry's
          reason is refreshed (`not exercised by <scenarios>; needs an oracle
          case`), the body to go back.

        `neither` candidates are not built or measured; their entries keep the
        worker's reason.  Returns the fix-round problems."""
        wave = _tool('wave')
        key = wave._addr_key
        ce = _tool('check_evidence')
        self._frida_got = {}
        self._frida_notes = {}
        self._wave_notes = {}
        self._frida_ran = None
        if self.dry_run:
            return []
        batch = self.load_state(self.state_path)['batches'][batch_id]
        cands = {key(a): sc for a, sc in (self._candidates or {}).items()}
        listed = self._probe_listed()
        try:
            touched = {key('0x%X' % a) for _p, a, _n in ce.touched(worktree, self.base)}
        except Exception as e:
            raise _StageError('frida', 'cannot tell which bodies the batch changed (%s); no row '
                                       'was measured; fix it and clear this exception'
                              % _one_line(repr(e), 200)) from None
        outcomes, fork_rows = {}, []
        for r in batch.get('rows', []):
            k = key(r['address'])
            if k not in listed:
                continue           # resolved by an earlier run of this batch
            if r.get('outcome') == 'fork-equal':
                continue           # decided by an earlier phase; the checks and the record
                #                    stage refuse a body that moved since
            outcomes[r['address']] = {'outcome': None, 'measured_body': None, 'case': None,
                                      'losing_kind': None}
            side, reason = cands.get(k, (None, None))
            if side == 'neither':
                # not built, not measured: the entry stays, its reason refreshed
                self._probe_list_edit(r['address'], reason=reason, stamp=self._probed_stamp(
                    batch_id, r, batch, [], hand=True))
                outcomes[r['address']]['outcome'] = 'kept'
                self.log('%s: %s stays on the probe list (candidate neither: %s)' % (
                    batch_id, r['address'], reason))
            elif side == 'fork' and k in touched and self._evidence_kind(
                    worktree, r['address']) == 'read-only' and _mb_evidence_kind(
                    worktree, self.base, wave._evidence_rel(r['address'])) in (None, 'read-only'):
                fork_rows.append(dict(r))
        self._probe_outcomes(batch_id, outcomes)
        if not fork_rows:
            return []
        if not self.frida:
            raise _StageError('frida', 'a probe batch is decided by the Frida phase and this run '
                                       'has none (--frida); nothing was measured or resolved; '
                                       'rerun with --frida and clear this exception')
        got_written = {}

        def commit(written, cites, findings):
            got_written['rows'] = list(written)
            got_written['cites'] = list(cites)
        self._frida_phase(batch_id, batch, worktree,
                          {r['address']: ('fork', '') for r in fork_rows},
                          rows=fork_rows, commit=commit)
        wkeys = {key(a): c for a, c in zip(got_written.get('rows', []),
                                           got_written.get('cites', []))}
        mb_sha = wave.merge_base_sha(worktree, self.base)
        problems, resolved, decided = [], [], []
        for r in fork_rows:
            a, k = r['address'], key(r['address'])
            status, detail = self._frida_got.get(k, ('skipped', 'no result'))
            ran = self._ran_scenarios()
            if status == 'equal' and k in wkeys:
                cite = wkeys[k]
                case = 'frida-%s-%s' % (batch_id, cite)
                pin = r.get('fork_base') or batch.get('fork_base') or wave.DEFAULT_FORK_BASE
                sentence = None
                try:
                    self._deciding_sentence(worktree, a, pin, case, cite)
                except Exception as e:     # the trace evidence is committed whatever happens
                    sentence = e
                    self.log('%s: %s: the deciding sentence could not be written (%r)' % (
                        batch_id, a, e))
                    problems.append('%s: the deciding sentence could not be written (%r); add '
                                    '`disagreed with WSO at %s: ...; case %s decides` under '
                                    '## WSO delta' % (a, e, pin, case))
                resolved.append((r, case, cite))
                continue
            if status == 'equal':
                why = self._wave_notes.get(k) or 'no reason recorded'
                note = 'equal to the exe but no trace evidence was written (%s); needs an ' \
                       'oracle case' % why
                outcome = 'kept'
            elif status == 'different':
                m = re.search(r'differs in ([A-Za-z0-9_]+)', detail)
                scenario = m.group(1) if m else (ran[0] if ran else '')
                note = 'fork body differs from the exe in %s: %s' % (scenario or 'a scenario',
                                                                       detail)
                self._probe_gap(batch_id, r, scenario, detail)
                outcome = 'found'
            elif status == 'not-exercised':
                names = (list(getattr(self, '_frida_consulted', None) or ran)
                         if detail.startswith(COVERAGE_NOT_HIT) else ran)
                note = 'not exercised by %s; needs an oracle case' % ', '.join(names)
                outcome = 'kept'
            elif status == 'skipped':
                note = 'no Frida hook (%s); needs an oracle case' % detail
                outcome = 'kept'
            else:
                note = 'not measured (%s: %s); needs an oracle case' % (status, detail)
                outcome = 'kept'
            # a body known to differ must not be re-adopted for a new scenario
            stamp = self._probed_stamp(batch_id, r, batch, ran, hand=outcome == 'found')
            if outcome == 'found':
                self._probe_list_edit(a, append=note, stamp=stamp)
            else:
                self._probe_list_edit(a, reason=note, stamp=stamp)
            self._probe_outcomes(batch_id, {a: {'outcome': outcome}})
            self.log('%s: %s stays on the probe list (%s)' % (batch_id, a, note))
            problems.append(self._probe_back_text(worktree, a, note))
        if resolved:
            self._commit_trace_evidence(
                batch_id, worktree, [r['address'] for r, _c, _s in resolved],
                'frida: trace evidence for %d row(s) measured equal (%s)' % (
                    len(resolved), ', '.join(sorted({sc for _r, _c, sc in resolved}))))
        for r, case, _scenario in resolved:
            a = r['address']
            rel = wave._evidence_rel(a)
            was = self._git_in(worktree, 'show', '%s:%s' % (mb_sha or self.base, rel))
            m = wave.KIND_RE.search(was.stdout) if was.returncode == 0 else None
            self._probe_outcomes(batch_id, {a: {
                'outcome': 'fork-equal', 'case': case,
                'losing_kind': m.group(1) if m else 'read-only',
                'measured_body': ce.body_digest(worktree, a, r.get('file'))}})
            self.log('%s: %s decided for the fork (case %s); recorded at the record stage'
                     % (batch_id, a, case))
        return problems

    # -- remeasure: a Frida-only pass over closed rows (Task 20 part B) ----

    ENGINE_GAPS_REL = 'docs/wso/engine-gaps.json'
    REMEASURE_LOG_REL = 'docs/wso/remeasure-log.json'

    @staticmethod
    def remeasure_chunks(picked, size):
        """`picked` in slices of at most `size` rows."""
        size = max(1, int(size))
        return [picked[i:i + size] for i in range(0, len(picked), size)]

    def remeasure_plan(self, picked, skipped, chunk, mode='', stamp=None):
        """The text of `wave.py remeasure --dry`: the rows, the chunks and the
        scenarios the coverage map picks for each; nothing is launched or
        written."""
        chunks = self.remeasure_chunks(picked, chunk)
        out = ['remeasure plan%s: %d row(s) in %d chunk(s) of at most %d' % (
            ' (%s)' % mode if mode else '', len(picked), len(chunks), chunk)]
        for i, part in enumerate(chunks, 1):
            rows = [p['row'] for p in part]
            scenarios, skip = self._coverage_scenarios('remeasure-plan', rows)
            out.append('chunk %d of %d: %d row(s); scenarios: %s' % (
                i, len(chunks), len(rows),
                'none (coverage says no scenario exercises any row)' if skip
                else ', '.join(scenarios)))
            for p in part:
                out.append('  %s %s (%s, verdict %s %s)' % (
                    p['row']['address'], p['row'].get('name', ''), p['batch'], p['side'], p['case']))
        for a, why in skipped:
            out.append('skipped %s: %s' % (a, why))
        out.append('nothing launched; the verdicts and the wave state are never touched')
        return '\n'.join(out) + '\n'

    def _remeasure_json_list(self, worktree, rel):
        path = os.path.join(worktree, rel)
        try:
            with open(path, encoding='utf-8') as f:
                data = json.load(f)
        except FileNotFoundError:
            return path, []
        if not isinstance(data, list):
            raise ValueError('%s is not a JSON list' % rel)
        return path, data

    def remeasure(self, picked, skipped, chunk=40, branch=None, mode='', stamp=None):
        """(exit code, summary text).  Measures the picked closed rows with
        the Frida phase (_frida_phase over the rows it is given: the coverage
        map's scenarios, the x86 build of the worktree (once per pass),
        frida_run under probe.py's locks, frida_verdicts, the dump guard) on a
        new branch p3/remeasure-<stamp> cut from the base, chunk by chunk.
        Per row: equal -> evidence rewritten to kind: trace (Part A's writer,
        the exemption text kept), different -> an entry in
        docs/wso/engine-gaps.json, anything else listed in the summary.  One
        commit per chunk; docs/wso/remeasure-log.json carries the pass (a stop
        or a failed phase too).  No verdict, state or ledger is written.  A
        stop (SIGTERM/SIGINT) kills the live process groups and taskkills the
        sandbox only when a probe was among them, like the driver's."""
        wave = _tool('wave')
        key = wave._addr_key
        stamp = stamp or datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
        branch = branch or 'p3/remeasure-%s' % stamp
        pid = 'remeasure-%s' % stamp
        if not picked:
            return 0, 'remeasure: no row to measure\n' + ''.join(
                'skipped %s: %s\n' % s for s in skipped)
        if subprocess.run(['git', '-C', self.repo, 'rev-parse', '--verify', '--quiet',
                           'refs/heads/%s' % branch], capture_output=True).returncode == 0:
            return 1, ('remeasure: branch %s exists already; a pass cuts its own new branch '
                       '(--branch NAME names an unused one)\n' % branch)
        for rel in (self.ENGINE_GAPS_REL, self.REMEASURE_LOG_REL):
            # a malformed file refuses before a branch is cut or any evidence rewritten
            r = self._git_in(self.repo, 'show', '%s:%s' % (self.base, rel))
            if r.returncode == 0:
                try:
                    ok = isinstance(json.loads(r.stdout), list)
                except ValueError:
                    ok = False
                if not ok:
                    return 1, 'remeasure: %s is not a JSON list; nothing was measured\n' % rel
        worktree, how = self.cut_worktree(self.repo, branch, self.base)
        if worktree is None:
            return 1, 'remeasure: %s\n' % how
        self.log('%s: worktree %s on %s (%s)' % (pid, worktree, branch, how))
        self.log('%s: the sandbox and capture locks serialise this pass with any other driver '
                 'using them; it may wait' % pid)
        chunks = self.remeasure_chunks(picked, chunk)
        tot = {'asked': len(picked), 'measured': 0, 'equal': 0, 'different': 0,
               'not_exercised': 0, 'other': 0, 'trace_evidence': 0, 'chunks': len(chunks),
               'chunks_done': 0}
        listed, stopped = [], None
        self._reuse_target = {}
        self._frida_cache_writes, cache_was = False, self.frida_cache
        self.frida_cache = False
        previous = {}
        _STOPPED.clear()
        for sig in (signal.SIGTERM, signal.SIGINT):
            try:
                previous[sig] = signal.signal(sig, self._on_stop)
            except ValueError:         # not the main thread: no handler
                pass

        def write_log(extra=None):
            log_path, log = self._remeasure_json_list(worktree, self.REMEASURE_LOG_REL)
            entry = dict(tot, stamp=stamp, mode=mode, branch=branch, at=_now(), **(extra or {}))
            log = [e for e in log if not (isinstance(e, dict) and e.get('stamp') == stamp)]
            log.append(entry)
            wave._save_json(log_path, log)
        try:
            for i, part in enumerate(chunks, 1):
                rows = [p['row'] for p in part]
                verdicts = {p['row']['address']: (p['side'], p['case']) for p in part}
                self._probe = {}
                got_written = {}

                def commit(written, cites, findings, got_written=got_written):
                    got_written['rows'] = list(written)
                    got_written['cites'] = list(cites)
                self.log('%s: chunk %d of %d: %d row(s)' % (pid, i, len(chunks), len(rows)))
                try:
                    self._frida_phase(pid, {'branch': branch, 'rows': rows}, worktree, verdicts,
                                      rows=rows, commit=commit)
                except _StageError as e:
                    stopped = 'chunk %d: %s' % (i, e.problem)
                    self.log('%s: %s' % (pid, stopped))
                    break
                written = got_written.get('rows', [])
                wkeys = {key(a) for a in written}
                gaps_path, gaps = self._remeasure_json_list(worktree, self.ENGINE_GAPS_REL)
                found = 0
                for r in rows:
                    status, detail = self._frida_got.get(key(r['address']), ('skipped', 'no result'))
                    if status in ('equal', 'different'):
                        tot['measured'] += 1
                    if status == 'equal':
                        tot['equal'] += 1
                        if key(r['address']) not in wkeys:
                            listed.append('%s equal but no trace evidence written: %s' % (
                                r['address'],
                                self._wave_notes.get(key(r['address'])) or 'no reason recorded'))
                    elif status == 'different':
                        tot['different'] += 1
                        found += 1
                        m = re.search(r'differs in ([A-Za-z0-9_]+)', detail)
                        gaps.append({'address': r['address'], 'name': r.get('name', ''),
                                     'scenario': m.group(1) if m else '', 'detail': detail,
                                     'found_by': pid, 'at': _now()})
                    elif status == 'not-exercised':
                        tot['not_exercised'] += 1
                        listed.append('%s not exercised (%s)' % (r['address'], detail))
                    else:
                        tot['other'] += 1
                        listed.append('%s %s (%s)' % (r['address'], status, detail))
                tot['trace_evidence'] += len(written)
                tot['chunks_done'] = i
                if found:
                    wave._save_json(gaps_path, gaps)
                write_log()
                rels = [wave._evidence_rel(a) for a in written] + [self.REMEASURE_LOG_REL] + (
                    [self.ENGINE_GAPS_REL] if found else [])
                msg = 'remeasure: trace evidence for %d row(s) (%s); %d finding(s)' % (
                    len(written), ', '.join(sorted(set(got_written.get('cites', [])))) or 'none',
                    found)
                self._git_in(worktree, 'add', '--', *rels)
                r = self._git_in(worktree, 'commit', '-q', '-m', msg, '--', *rels)
                if r.returncode != 0:
                    self.log('%s: chunk %d not committed: %s' % (
                        pid, i, _one_line((r.stderr or '') + (r.stdout or ''))))
        except _Stop:
            stopped = 'stopped by a signal; its live process groups were killed'
            self.log('%s: %s' % (pid, stopped))
        finally:
            for sig, handler in previous.items():
                signal.signal(sig, handler)
            _STOPPED.clear()
            self._reuse_target = None
            self._frida_cache_writes, self.frida_cache = True, cache_was

            def close_running(s):
                s['running'].pop(pid, None)
                if (s.get('current') or {}).get('batch') == pid:
                    s['current'] = None
            self._update_status(close_running)
        if stopped:
            write_log({'stopped': stopped})
            self._git_in(worktree, 'add', '--', self.REMEASURE_LOG_REL)
            self._git_in(worktree, 'commit', '-q', '-m',
                         'remeasure: pass %s stopped after %d of %d chunk(s)' % (
                             stamp, tot['chunks_done'], tot['chunks']), '--',
                         self.REMEASURE_LOG_REL)
        lines = ['remeasure %s: %d asked, %d measured (%d equal, %d different), %d not exercised, '
                 '%d other; %d trace evidence file(s) written in %d of %d chunk(s)' % (
                     stamp, tot['asked'], tot['measured'], tot['equal'], tot['different'],
                     tot['not_exercised'], tot['other'], tot['trace_evidence'],
                     tot['chunks_done'], tot['chunks'])]
        lines += ['  ' + l for l in listed]
        lines += ['skipped %s: %s' % s for s in skipped]
        if stopped:
            lines.append('STOPPED: %s' % stopped)
            dirty = self._git_in(worktree, 'status', '--porcelain').stdout.strip()
            if dirty:
                lines.append('uncommitted in the worktree (look before landing):\n' + dirty)
        lines.append('branch: %s (worktree %s); gate it (tools/gates.sh --base %s) and land it' % (
            branch, worktree, self.base))
        return (1 if stopped else 0), '\n'.join(lines) + '\n'

    def _merged(self, batch_id, verdicts):
        """The state's verdicts plus the report's, as (side, case)."""
        key = _tool('wave')._addr_key
        batch = self.load_state(self.state_path)['batches'][batch_id]
        out = {key(a): _side_case(v) for a, v in batch.get('verdicts', {}).items()}
        out.update({key(a): sc for a, sc in verdicts.items()})
        if self.kind == 'probes':
            # the rows the phase decided for the fork are verdicts-to-be: review is
            # mandatory (fork rows) and lists them
            out.update({key(r['address']): ('fork', r.get('case', ''))
                        for r in batch.get('rows', []) if r.get('outcome') == 'fork-equal'})
        return out

    def _frozen_rows(self, batch_id):
        """[(address, name)] of the rows the Frida phase measured, whose bodies
        a fix round must leave alone: an import batch's rows with a
        measured_body, a probes batch's rows the phase wrote evidence for
        (the status's frida_written), a verdicts batch's rows the
        phase measured equal (_frida_got)."""
        key = _tool('wave')._addr_key
        batch = self.load_state(self.state_path)['batches'].get(batch_id) or {}
        rows = batch.get('rows', [])
        if self.kind == 'import':
            want = {key(r['address']) for r in rows if r.get('measured_body')}
        elif self.kind == 'probes':
            want = set(self._frida_lines(batch_id))
        else:
            # measured: 'frida' reaches the state only at record time, after the
            # fix rounds; the phase's own result says which rows it measured equal
            want = {k for k, (st, _d) in self._frida_got.items() if st == 'equal'}
        named = {key(r['address']): (r['address'], r.get('name', '')) for r in rows}
        return [named.get(k, ('0x' + k.upper(), '')) for k in sorted(want)]

    def _fix(self, batch_id, batch, worker, worktree, stage, problems, brief, label):
        prompt = fix_prompt(batch_id, stage, batch['branch'], brief, problems, self.kind,
                            frozen=self._frozen_rows(batch_id))
        self._stage(batch_id, label)
        return self._run(worker, prompt, worktree, batch_id, label)

    def _review(self, batch_id, batch, worktree, merged, why, label):
        """(blocking findings, None) or (None, outcome)."""
        self._stage(batch_id, label)
        d = self._git('diff', '%s...%s' % (self.base, batch['branch']))
        if d.returncode != 0:
            return None, self._exception(batch_id, 'review', [
                'git diff %s...%s failed: %s' % (self.base, batch['branch'],
                                                 d.stderr.strip())])
        path = os.path.join(self.driver_dir, batch_id, 'review.diff')
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(d.stdout)
        brief = REVIEW_BRIEF.format(
            id=batch_id, branch=batch['branch'], base=self.base, diff=path,
            why=why, verdicts='\n'.join(
                '- verdict 0x%s %s case %s' % (k.upper(), side, case)
                for k, (side, case) in sorted(merged.items()))
            or '(none: an import batch has no verdicts)')
        res = self._run(self._reviewer_of(self._pool_name()), brief, worktree,
                        batch_id, label)
        out = self._settle(batch_id, res, label, worktree)
        if out:
            return None, out
        return blocking_findings(res.report), None

    def _classes(self, batch_id, batch):
        counts = {}
        for r in batch.get('rows', []):
            c = r.get('class') or 'unclassed'
            counts[c] = counts.get(c, 0) + 1
        moved = self._imported.get(batch_id)
        if moved:
            counts['ours-stub'] = counts.get('ours-stub', 0) + moved
        return counts

    def _git(self, *args):
        return subprocess.run(['git', '-C', self.repo, *args],
                              capture_output=True, text=True)

    def _is_tracked(self, name):
        """True when `name` is a file tracked in the driver's worktree."""
        return self._git('ls-files', '--error-unmatch', '--', name
                         ).returncode == 0

    def _commits(self, branch):
        """'<merge-base>..<tip> (n commits)' of the batch branch over base."""
        mb = self._git('merge-base', self.base, branch).stdout.strip()
        tip = self._git('rev-parse', '--verify', branch).stdout.strip()
        if not mb or not tip:
            return 'unknown (%s..%s)' % (self.base, branch)
        n = self._git('rev-list', '--count', '--no-merges', '%s..%s' % (mb, tip)).stdout.strip()
        return '%s..%s (%s commit%s)' % (mb[:8], tip[:8], n, '' if n == '1' else 's')

    def _worker_of(self, name):
        return self.workers_by_pool.get(name)

    def _reviewer_of(self, name):
        return self.reviewers_by_pool.get(name)

    def _figure_kw(self):
        """ledger_entry's pool, wall, tokens and tool_calls (R10, R61)."""
        f = self._figures() or {}
        return dict(pool=self._pool_name(), wall=f.get('wall'),
                    tokens=f.get('tokens'), tool_calls=f.get('tool_calls'),
                    transcript=list(self._ctx.transcript_lines))

    def _ledger_for(self, batch_id, verdicts, review):
        batch = self.load_state(self.state_path)['batches'][batch_id]
        self._append_ledger(ledger_entry(
            dict(batch, id=batch_id), verdicts, self._classes(batch_id, batch),
            review, self._commits(batch['branch']), self._credits(),
            ignored=self._ignored, undecided=self._undecided,
            **self._figure_kw()))

    def _record_pool(self, batch_id, name):
        """Write batch['pool'] = name into the state entry (ruling R10)."""
        with self._lock:
            state = self.load_state(self.state_path)
            batch = state['batches'].get(batch_id)
            if batch is not None and batch.get('pool') != name:
                batch['pool'] = name
                _tool('wave').save_state(self.state_path, state)

    UNION_LINE = 'oracle/CMakeLists.txt merge=union'

    def _union_merge_attributes(self):
        """Make oracle/CMakeLists.txt merge as a union (ruling R64): every
        batch with passing probes adds its case line after the same anchor,
        so parallel batches would conflict.  Written once, to the common git
        dir's info/attributes, only when the exact line is missing."""
        r = self._git('rev-parse', '--git-common-dir')
        if r.returncode != 0 or not r.stdout.strip():
            return
        common = r.stdout.strip()
        if not os.path.isabs(common):
            common = os.path.join(self.repo, common)
        path = os.path.join(common, 'info', 'attributes')
        try:
            with open(path, encoding='utf-8') as f:
                text = f.read()
        except FileNotFoundError:
            text = ''
        except OSError as e:
            self.log('git attributes not read: %r' % (e,))
            return
        if self.UNION_LINE in text.splitlines():
            return
        try:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'a', encoding='utf-8') as f:
                f.write(('' if not text or text.endswith('\n') else '\n')
                        + self.UNION_LINE + '\n')
        except OSError as e:
            self.log('git attributes not written: %r' % (e,))

    def _sync_branch(self, branch, worktree):
        """None, or why not: with batches in parallel the base may have
        moved since this branch was cut, and close only fast-forwards, so
        the base is merged into the branch first (the files are disjoint;
        a no-op when the base is already an ancestor).  The base is pulled
        first (ruling R17).  Under the lock."""
        return self._merge_base_into(branch, worktree)[0]

    def _merge_base_into(self, branch, worktree, when='the close', pull=True,
                         message=None):
        """(why or None, conflicting paths): the merge _sync_branch does,
        also run before the mechanical checks (`when`, `pull` False: the
        close alone pulls; `message`: the merge commit's subject).  A merge
        that conflicts is aborted; the paths it conflicted in come back."""
        if pull:
            why = self.sync_base(self.repo, self.base)
            if why:
                return why, []
        if self._git('merge-base', '--is-ancestor', self.base,
                     branch).returncode == 0:
            return None, []
        self._union_merge_attributes()
        how = ['-m', message + '\n\n' + STATE_COMMIT_TRAILER + '\n',
               '--cleanup=strip'] if message else ['--no-edit', '--cleanup=strip']
        r = subprocess.run(['git', '-C', worktree, 'merge', *how, '-q',
                            self.base], capture_output=True, text=True)
        if r.returncode != 0 and self._only_map_conflicts(worktree):
            # A worker's own docs/MAP.md edit: the close regenerates the map
            # on the merged tree anyway, so the base's copy stands.
            r = self._git_in(worktree, 'checkout', '--theirs', '--', 'docs/MAP.md')
            if r.returncode == 0:
                r = self._git_in(worktree, 'add', '--', 'docs/MAP.md')
            if r.returncode == 0:
                r = self._git_in(worktree, 'commit', '-q', '--no-edit',
                                '--cleanup=strip')
        if r.returncode == 0:
            self.log('%s: %s merged into it before %s' % (branch, self.base, when))
            return None, []
        conflicts = self._git_in(worktree, 'diff', '--name-only',
                                 '--diff-filter=U').stdout.split()
        subprocess.run(['git', '-C', worktree, 'merge', '--abort'],
                       capture_output=True, text=True)
        return 'merging %s into %s failed: %s' % (
            self.base, branch, _one_line((r.stderr or '') + (r.stdout or ''))), conflicts

    def _merge_before_checks(self, batch_id, worktree):
        """A problem list: [] when `first` was merged into the batch branch
        (or needed no merge, or the worktree is dirty with the worker's own
        files), else the one coordinator problem.  The checks must see the
        tree the close will gate (Task 26); a merge conflict is the
        coordinator's, never the worker's (the worker may not merge).  A
        dirty driver-owned file is restored and reported (a selfcheck that
        could not find the live state used to copy it in), a driver-owned
        file hidden behind a skip-worktree/assume-unchanged flag is un-hidden
        first and restored with the rest, and a driver-owned file in the
        worker's own diff was committed by the worker; none of them stops
        the merge (Task 33)."""
        problems = []
        try:
            branch = self.load_state(self.state_path)['batches'][batch_id]['branch']
        except (OSError, KeyError, ValueError, TypeError):
            return problems
        r = self._git_in(worktree, 'diff', '--name-only',
                         _merge_base(worktree, self.base), 'HEAD')
        if r.returncode == 0:
            problems += ["%s is the driver's; revert your commit's change to it "
                         '(git revert or a new commit putting the merge-base copy '
                         'back)' % p for p in r.stdout.split() if p in DRIVER_OWNED]
        # A driver-owned file hidden behind an index flag (S = skip-worktree,
        # h = assume-unchanged; the i021/i022/i023 workers set one after
        # copying the live state in) keeps `status` clean while its content
        # differs from HEAD, and the merge below would refuse on it.  Clear
        # the flag, then treat the file as dirty like any other driver-owned
        # modification (Task 33).
        hidden = []
        r = self._git_in(worktree, 'ls-files', '-v', '--', *DRIVER_OWNED)
        if r.returncode == 0:
            for line in r.stdout.splitlines():
                flag, _, path = line.partition(' ')
                if flag in ('S', 'h') and path in DRIVER_OWNED:
                    hidden.append((path, flag))
        if hidden:
            with self._writing():
                for p, flag in hidden:
                    # One negation per call: a single `--no-skip-worktree
                    # --no-assume-unchanged` invocation leaves skip-worktree
                    # set (git 2.43.0), so the merge below would still refuse.
                    self._git_in(worktree, 'update-index',
                                 '--no-skip-worktree' if flag == 'S'
                                 else '--no-assume-unchanged', '--', p)
        st = self._git_in(worktree, 'status', '--porcelain', '-uall')
        dirty = [(l[:2] == '??', l[3:].split(' -> ')[-1].strip('"'))
                 for l in st.stdout.splitlines() if l.strip()]
        dirty = [(u, p) for u, p in dirty if p != REPORT_DRAFT]
        # A hidden file whose content matches HEAD shows in no status line;
        # the flag alone still counts as the worker's doing.
        dirty += [(False, p) for p, _ in hidden if p not in {q for _, q in dirty}]
        if st.returncode != 0 or any(p not in DRIVER_OWNED for _, p in dirty):
            return problems           # the uncommitted-paths problem covers it
        if dirty:
            with self._writing():
                for untracked, p in dirty:
                    if untracked:
                        try:
                            os.unlink(os.path.join(worktree, p))
                        except OSError:
                            pass
                    else:
                        self._git_in(worktree, 'checkout', '--', p)
                    self.log('%s: %s was modified by the worker; restored (driver-owned)'
                             % (batch_id, p))
            problems += ['%s was modified by the worker; restored (driver-owned)'
                         % p for _, p in dirty]
        r = self._git_in(worktree, 'merge-base', '--is-ancestor', self.base, 'HEAD')
        if r.returncode != 1:          # 0: nothing moved; else not a repo we can read
            return problems
        with self._writing():
            why, conflicts = self._merge_base_into(
                branch, worktree, when='the checks', pull=False,
                message='merge %s into %s before the checks' % (self.base, branch))
        if not why:
            return problems
        if conflicts:
            problems.append('%s conflicts with this branch in %s; the coordinator '
                            'resolves it' % (self.base, ', '.join(sorted(conflicts))))
        else:
            problems.append('%s could not be merged into this branch (%s); the '
                            'coordinator resolves it' % (self.base, why))
        return problems

    LANDING_POLL = 0.25    # seconds between tries at the landing lock

    def _landing_path(self):
        """<git common dir>/iwd2r-landing.lock: one per repository, so every
        driver process (both queues) lands on `first` one at a time."""
        r = self._git('rev-parse', '--path-format=absolute', '--git-common-dir')
        common = r.stdout.strip() if r.returncode == 0 and r.stdout.strip() \
            else os.path.join(self.repo, '.git')
        return os.path.join(common, 'iwd2r-landing.lock')

    def _note(self, batch_id, note):
        """Set (or clear, with None) the status note of the batch's stage."""
        def apply(s):
            for entry in (s.get('current'), s['running'].get(batch_id)):
                if isinstance(entry, dict) and (entry.get('batch') in (None, batch_id)):
                    if note is None:
                        entry.pop('note', None)
                    else:
                        entry['note'] = note
        self._update_status(apply)

    @contextlib.contextmanager
    def _landing_lock(self, batch_id):
        """Hold the repository's landing lock over merge-first, gate, map,
        fast-forward and push.  While another driver holds it the status
        shows the close stage with a waiting note; a stop while waiting
        raises _Stop, and the lock is released however the block ends.  The
        gate's own lock (tools/gates.sh, .gates.lock) is a different file
        and stays: it serialises gates for every caller of gates.sh."""
        path = self._landing_path()
        noted = False
        try:
            with contextlib.ExitStack() as stack:
                while True:
                    if self._stopping:
                        raise _Stop()
                    try:
                        stack.enter_context(_tool('filelock').hold(
                            path, 'driver landing', timeout=self.LANDING_POLL))
                        break
                    except TimeoutError:
                        if not noted:
                            noted = True
                            self.log('%s: waiting for the landing lock' % batch_id)
                            self._note(batch_id, WAITING_LANDING_NOTE)
                if noted:
                    noted = False
                    self._note(batch_id, None)
                yield
        finally:
            if noted:              # a stop (or an error) while waiting
                self._note(batch_id, None)

    STOP_WAIT = 30     # seconds a stop waits for a git write in progress

    @contextlib.contextmanager
    def _writing(self):
        """A git write is in progress on this thread: a stop waits for it
        (bounded by STOP_WAIT), and a stop that lands on this very thread
        waits for the write's end and is raised there."""
        tid = threading.get_ident()
        with self._write_cv:
            self._writers[tid] = self._writers.get(tid, 0) + 1
        try:
            yield
        finally:
            with self._write_cv:
                n = self._writers[tid] - 1
                if n:
                    self._writers[tid] = n
                else:
                    del self._writers[tid]
                self._write_cv.notify_all()
                deferred = not n and self._stop_deferred == tid
            if deferred:
                self._stop_deferred = None
                raise _Stop()

    def _close_sandbox_after_stop(self):
        """A probe killed mid-run leaves its Windows Sandbox open (it ends on
        its in-box timeout): taskkill the sandbox processes so a restart does
        not hit "already running".  One line names what was killed and what
        failed; one failing name skips neither the rest nor the line."""
        try:
            done = self._taskkill()
            killed = [n for n, rc in done if rc == 0]
            failed = [n for n, rc in done if rc is None]
            self.log('stop: killed the live process groups and taskkilled the '
                     'sandbox (killed: %s; failed: %s)'
                     % (', '.join(killed) or 'none', ', '.join(failed) or 'none'))
        except Exception:
            pass

    def _on_stop(self, signum, _frame):
        """SIGTERM/SIGINT: kill the workers, let a git write in progress
        finish (bounded), then raise _Stop.  Only locks that are re-entrant
        are taken here: the signal may land while this thread holds them."""
        self._stopping = True
        killed = kill_live_groups()
        with _LIVE_LOCK:
            probe_killed = bool(set(killed) & _PROBE_PIDS)
        if probe_killed:               # a stop with only Claude workers live leaves boxes alone
            self._close_sandbox_after_stop()
        with self._write_cv:
            if threading.get_ident() in self._writers:
                # Raised on this thread when its write ends (the main
                # thread's cut, adoption and recompare are writes too).
                self._stop_deferred = threading.get_ident()
                return
            deadline = time.monotonic() + self.STOP_WAIT
            while self._writers:
                left = deadline - time.monotonic()
                if left <= 0:
                    break
                self._write_cv.wait(left)
        raise _Stop()

    def _git_in(self, worktree, *args):
        return subprocess.run(['git', '-C', worktree, *args],
                              capture_output=True, text=True)

    def _only_map_conflicts(self, worktree):
        r = self._git_in(worktree, 'diff', '--name-only', '--diff-filter=U')
        return r.returncode == 0 and r.stdout.split() == ['docs/MAP.md']

    def _regen_map(self, worktree):
        """None, or why not: regenerate docs/MAP.md on the merged tree
        (batches no longer carry it, so parallel closes cannot conflict)
        and commit it by name when it changed; `gen_map.py --check` passes
        after the fast-forward.  Under the lock."""
        # The worktree's own tool, the one the gate's check runs, so the two
        # cannot disagree.
        gen = os.path.join(worktree, 'tools', 'gen_map.py')
        if not os.path.isfile(os.path.join(worktree, 'docs', 'MAP.md')):
            return None        # no map to keep current
        if subprocess.run([sys.executable, '-I', gen, '--repo', worktree,
                           '--check'], capture_output=True,
                          text=True).returncode == 0:
            return None        # current already (the commit line aside)
        r = subprocess.run([sys.executable, '-I', gen, '--repo', worktree],
                           capture_output=True, text=True)
        if r.returncode != 0:
            return _one_line((r.stderr or '') + (r.stdout or '')) or \
                'gen_map.py exit %s' % r.returncode
        st = self._git_in(worktree, 'status', '--porcelain', '--', 'docs/MAP.md')
        if st.returncode != 0:
            return _one_line(st.stderr) or 'git status failed'
        if not st.stdout.strip():
            return None
        r = self._git_in(worktree, 'add', '--', 'docs/MAP.md')
        if r.returncode == 0:
            r = self._git_in(
                worktree, 'commit', '-q', '-m',
                'map: regenerate docs/MAP.md on the merged tree\n\n%s\n'
                % STATE_COMMIT_TRAILER, '--', 'docs/MAP.md')
        if r.returncode != 0:
            return _one_line((r.stderr or '') + (r.stdout or '')) or \
                'exit %s' % r.returncode
        return None

    def _commit_state(self, batch_id):
        """After a close, before the push (rulings R66, R71): commit the
        wave state, the regenerated scorecard and the probe list (ruling
        R73, when it exists), and only those, on the base, in the
        worktree that has it checked out, staged by name.  The import
        list goes with either queue's state, and the import queue
        commits its state file (no scorecard).  Skipped in a dry run, with nothing to
        commit, or when a file lies outside that worktree.  Under the
        lock.  Whatever goes wrong is logged, those paths are unstaged and
        the error text is returned; the push still happens (the close has
        landed) and the caller raises the `state` exception (Task 30)."""
        if self.dry_run:
            return None
        rels = []
        try:
            err = self._commit_state_files(batch_id, rels)
        except Exception as e:
            err = 'raised %r' % (e,)
        if err is None:
            return None
        self.log('%s: the state commit failed: %s' % (batch_id, err))
        try:
            wt = self.worktree_for(self.repo, self.base)
            if wt and rels:
                subprocess.run(['git', '-C', wt, 'reset', '-q', '--', *rels],
                               capture_output=True, text=True)
        except Exception as e:
            self.log('%s: state commit cleanup failed: %r' % (batch_id, e))
        return err

    STATE_LANDED = ('the batch landed on first without its state; fix the cause, then clear '
                    'this exception so the retry commits and pushes the state')

    @staticmethod
    def _push_problems(err, state_err):
        """The push exception's problems; a failed state commit is named too, so
        clearing it without fixing that cause is a visible choice."""
        out = ['git push failed: %s' % err]
        if state_err:
            out.append('the state commit also failed: %s' % state_err)
        return out

    def _state_failed(self, batch_id, err):
        """The `state` exception of a close whose state commit failed."""
        return self._exception(batch_id, 'state', [err, self.STATE_LANDED])

    def _commit_state_files(self, batch_id, rels):
        """None, or why not; `rels` is filled with the paths staged."""
        wt = self.worktree_for(self.repo, self.base)
        if wt is None:
            return 'no worktree has %s' % self.base
        top = os.path.realpath(wt)
        if self.kind == 'verdicts':
            # disagreements.json too: add and diff --cached skip it unchanged
            paths = [self.state_path, self.report_path, self.probe_path,
                     self.disagreements_path, self.triage_path,
                     os.path.join(os.path.dirname(self.disagreements_path),
                                  'coverage.pending.json')]
        elif self.kind == 'probes':
            # its own state, the wave state the rows were resolved into, the
            # scorecard (its `probes:` line), the probe list and the gaps found
            paths = [self.state_path, self.wave_state_path, self.report_path,
                     self.probe_path, self.disagreements_path,
                     os.path.join(self.repo, self.ENGINE_GAPS_REL)]
        else:    # the import queue: its state (never a scorecard)
            paths = [self.state_path]
        paths.append(self.import_list_path)   # ours-stub rows leave wave 1 for it
        for path in paths:
            if not os.path.exists(path):
                continue
            rel = os.path.relpath(os.path.realpath(path), top)
            if rel != os.pardir and not rel.startswith(os.pardir + os.sep) \
                    and rel not in rels:
                rels.append(rel)
        if not rels:
            return None

        def git(*args):
            return subprocess.run(['git', '-C', wt, *args],
                                  capture_output=True, text=True)

        def fail(r):
            return _one_line((r.stderr or '') + (r.stdout or '')) or \
                'exit %s' % r.returncode

        r = git('add', '--', *rels)
        if r.returncode != 0:
            return fail(r)
        if git('diff', '--cached', '--quiet', '--', *rels).returncode == 0:
            return None
        if self.kind == 'probes':
            rows = self.load_state(self.state_path)['batches'][batch_id]['rows']
            msg = 'probe-state: close %s (resolved %d, kept on list %d)\n\n%s\n' % (
                batch_id, sum(1 for r in rows if r.get('outcome') == 'resolved'),
                sum(1 for r in rows if r.get('outcome') in ('kept', 'found')),
                STATE_COMMIT_TRAILER)
            r = git('commit', '-q', '-m', msg, '--', *rels)
            return fail(r) if r.returncode != 0 else None
        sides = [v.get('side') for v in self.load_state(self.state_path)[
            'batches'][batch_id]['verdicts'].values()]
        msg = '%s: close %s (ours %d, fork %d, neither %d)\n\n%s\n' % (
            'wave-state' if self.kind == 'verdicts' else 'import-state',
            batch_id, sides.count('ours'), sides.count('fork'),
            sides.count('neither'), STATE_COMMIT_TRAILER)
        r = git('commit', '-q', '-m', msg, '--', *rels)
        return fail(r) if r.returncode != 0 else None

    def _probe_ledger_lines(self, batch):
        """The probe batch's tallies for its ledger entry: resolved, kept on
        the list, findings (the fork body differs from the exe)."""
        rows = batch.get('rows', [])
        res = [r for r in rows if r.get('outcome') == 'resolved']
        found = [r for r in rows if r.get('outcome') == 'found']
        kept = [r for r in rows if r.get('outcome') in ('kept', 'found')]
        lines = '- probes: resolved %d, kept on list %d, findings %d\n' % (
            len(res), len(kept), len(found))
        lines += ''.join('- probe resolved %s: case %s\n' % (r['address'], r.get('case'))
                         for r in res)
        lines += ''.join('- probe finding %s: fork body differs from the exe\n' % r['address']
                         for r in found)
        return lines

    def _wip_tip(self, batch_id, branch):
        """The branch tip's sha when it is the driver's own unpushed WIP
        commit: the one this batch's park recorded, with WIP_MESSAGE, not
        in the base and on no remote branch.  Else None."""
        entry = self._read_status()['batches'].get(batch_id)
        wip = entry.get('wip') if isinstance(entry, dict) else None
        tip = self._git('rev-parse', '--verify', branch).stdout.strip()
        if not wip or tip != wip:
            return None
        if self._git('log', '-1', '--format=%s', tip).stdout.strip() != WIP_MESSAGE:
            return None
        if self._git('merge-base', '--is-ancestor', tip, self.base).returncode == 0:
            return None
        if self._git('branch', '-r', '--contains', tip).stdout.strip():
            return None
        return tip

    def _undo_wip(self, worktree, tip):
        """`git reset --soft HEAD~1` of the driver's own WIP commit, made
        in the batch worktree when that worktree's HEAD is `tip`: `tip`, or
        None when nothing was undone."""
        if not worktree or os.path.realpath(worktree) == os.path.realpath(
                self.repo):
            return None

        def git(*args):
            return subprocess.run(['git', '-C', worktree, *args],
                                  capture_output=True, text=True)
        if git('rev-parse', 'HEAD').stdout.strip() != tip:
            return None
        r = git('reset', '--soft', 'HEAD~1')
        if r.returncode != 0:
            self.log('WIP commit not undone: %s' % _one_line(
                (r.stderr or '') + (r.stdout or '')))
            return None
        return tip

    def _resume_preamble(self, batch_id, batch, why, worktree=None):
        if self.kind == 'probes':
            tip = self._wip_tip(batch_id, batch['branch'])
            if tip is not None:
                tip = self._undo_wip(worktree, tip)
            return RESUME_PREAMBLE_PROBES.format(
                id=batch_id, why=why, branch=batch['branch'], base=self.base,
                wip=WIP_MESSAGE, reset='' if tip is None else (
                    ' The driver undid its own `%s` commit before resuming: the previous '
                    'work is staged in the worktree, no git command needed. Commit it with '
                    'proper messages and its `Imported-From:` trailers.' % WIP_MESSAGE))
        key = _tool('wave')._addr_key
        recorded = sorted(batch.get('verdicts', {}).items(),
                          key=lambda kv: key(kv[0]))
        tip = self._wip_tip(batch_id, batch['branch'])
        # Ruling R44: the driver, not the worker, undoes its own WIP commit
        # (a worker's `git reset` is a history exception), so an adopted fork
        # body in it can get its Imported-From: trailer.
        if tip is not None:
            tip = self._undo_wip(worktree, tip)
        reset = '' if tip is None else (
            ' The driver undid its own `%s` commit before resuming: the '
            'previous work is staged in the worktree, no git command needed. '
            'Commit it with proper messages and its `Imported-From:` '
            'trailers.' % WIP_MESSAGE)
        return RESUME_PREAMBLE.format(
            id=batch_id, why=why, branch=batch['branch'], base=self.base,
            wip=WIP_MESSAGE, reset=reset, verdicts='\n'.join(
                '- verdict %s %s case %s' % ((a,) + _side_case(v))
                for a, v in recorded) or '(none yet)')

    def run_batch(self, batch_id, resume=None):
        """One assigned batch, start to end: 'closed', 'exception' or
        'parked' ('checked' for a dry run whose checks passed).  With
        `resume` (why the batch is taken up again), the brief opens with
        a `Resume:` preamble listing the verdicts already recorded.  An
        OSError in a stage is that stage's exception (ruling R44)."""
        try:
            return self._run_batch(batch_id, resume)
        except _StageError as e:
            return self._exception(batch_id, e.stage, [e.problem])
        except _LaunchError as e:
            reason = 'launch failed: %s' % e.error
            out = self._park(batch_id, reason, e.cwd)
            self._halt_run(reason)
            return out

    def _run_batch(self, batch_id, resume):
        self._runs = []
        self._worker_ran = []
        self._ignored = {}
        self._undecided = {}
        self._probe = {}
        self._frida_got = {}
        self._frida_notes = {}
        self._ctx.usage = {'input': 0, 'cached': 0, 'output': 0, 'tool_calls': 0}
        self._ctx.final = None
        self._ctx.transcript_lines = []
        self._ctx.t0 = self._started.pop(batch_id, None)
        if self._ctx.t0 is None:
            self._ctx.t0 = self._clock()
        state = self.load_state(self.state_path)
        batch = state['batches'].get(batch_id)
        stage = self._retry_stage.pop(batch_id, None)
        if batch is None:
            return self._exception(batch_id, 'state', [
                'no batch %s in %s' % (batch_id, self.state_path)])
        if batch['status'] != 'assigned':
            if stage in ('push', 'state') and batch['status'] == 'closed':
                # A cleared push or state exception: the state commit and the
                # push again, nothing else.
                return ('exception' if self._settle_closed(batch_id)
                        else 'closed')
            self.log('%s is %s; nothing to do' % (batch_id, batch['status']))
            return 'closed'
        # The pool: the one this run assigned, else the one the state
        # names, else the first.
        names = [p.name for p in self.pools]
        name = self._ctx.pool or batch.get('pool')
        if name not in names:
            name = names[0]
        self._ctx.pool = name
        self._record_pool(batch_id, name)
        worktree = self.worktree_for(self.repo, batch['branch'])
        if worktree is None or not os.path.isdir(worktree):
            # A removed directory stays listed (prunable) until a prune.
            # The exception also ends this batch's park and current.
            gone = '' if worktree is None else ' (%s is gone)' % worktree
            return self._exception(batch_id, 'worktree', [
                '%s has no worktree%s; run `git worktree prune`, then `git '
                'worktree add <path> %s`, then clear this exception' % (
                    batch['branch'], gone, batch['branch'])])
        if stage in ('record', 'close') and not self.dry_run:
            # A cleared record or close exception: straight to that stage on
            # the worktree and the verdicts already recorded; the worker is
            # not run again (ruling R71).
            self.log('%s: retried at its %s stage; the worker and the review '
                     'are not run again' % (batch_id, stage))
            if self.kind == 'verdicts':
                self._undecided = self._saved_undecided(batch_id, batch)
            if self.kind == 'import' and (
                    self.rerun_frida or self._retry_frida.pop(batch_id, False)):
                # With --rerun-frida (Task 63) the phase runs again on the
                # worktree as the worker left it: what the close check asks
                # for -- a trace, or the unmeasured WSO marking, for a row
                # the batch imported anyway -- only that phase writes, and
                # the worker is still not run again.  The kept set a report
                # parsed is rebuilt from the recorded outcomes.
                self._kept = {r['address']: 'recorded' for r in batch.get('rows', [])
                              if r.get('outcome') == 'kept'}
                findings = self._frida_phase(batch_id, batch, worktree, {})
                if findings:
                    # a parking finding needs the worker to apply it
                    return self._exception(batch_id, 'frida', list(findings))
            return self._land(
                batch_id, batch, worktree,
                self._saved_verdicts(batch_id, batch) if stage == 'record' else {},
                (False, 'retry at %s: worker and review not run again' % stage))
        kind = self._retry_kind.pop(batch_id, None)
        recorded = self._retry_problems.pop(batch_id, None)
        if stage == 'recheck' and self.dry_run:
            stage = 'checks'
        if kind and not self.dry_run:
            self._update_status(lambda s: (
                s['batches'].setdefault(batch_id, {}).__setitem__('retry_kind', kind),
                (s.get('cleared') or {}).pop(batch_id, None)))
        if stage == 'recheck':
            return self._recheck(batch_id, batch, worktree, name, recorded)
        if self.kind == 'import':
            # the worker runs again and so does the phase: nothing recorded by
            # an earlier run (outcome kept, measured bodies) carries over
            self._clear_measured(batch_id)
        self._stage(batch_id, 'brief')
        try:
            rc, brief = self.render_brief(
                self.template_path, self.state_path, batch_id,
                bundle_dir=os.path.join(self.driver_dir, 'bundles', batch_id),
                repo=self.repo, base=self.base)
        except Exception as e:
            rc, brief = 1, 'render_brief raised %r' % (e,)
        if rc != 0:
            return self._exception(batch_id, 'brief', [brief.strip()])
        if resume:
            brief = self._resume_preamble(batch_id, batch, resume,
                                          worktree) + brief
            self.log('%s: resumed (%s)' % (batch_id, resume))
        worker = (FakeWorker(dry_report(batch)) if self.dry_run
                  else self._worker_of(name))
        if not self.dry_run and self.kind == 'verdicts':
            brief = self._oracle_phase(batch_id, batch, worktree) + brief

        # The worker, the mechanical checks, one fix round.
        self._stage(batch_id, 'worker')
        res = self._run(worker, brief, worktree, batch_id, 'worker')
        out = self._settle(batch_id, res, 'worker', worktree)
        if out:
            return out
        return self._after_worker(batch_id, batch, worktree, name, worker, brief,
                                  res.report)

    def _after_worker(self, batch_id, batch, worktree, name, worker, brief, report,
                      recorded=None):
        """Everything after a worker's (or a recheck's) report: the checks,
        the Frida phase, one fix round, the review, the landing.  `recorded`
        (a recheck): the problems of the cleared exception; the same
        problems again raise it again with no fix round."""
        verdicts, problems = self._check(batch_id, worktree, report)
        if problems and problems[0].startswith(COORDINATOR_PROBLEMS):
            # not the worker's to fix (it may not merge): no fix round
            return self._exception(batch_id, 'checks', problems)
        # Frida runs once, here, clean check or not; what it finds joins the
        # one fix round (ruling R58).
        problems = problems + (self._probe_phase(batch_id, batch, worktree)
                               if self.kind == 'probes' else
                               self._frida_phase(batch_id, batch, worktree, verdicts))
        if recorded is not None and problems and \
                [p for p in problems if p != UNCHANGED_PROBLEM] == \
                [p for p in recorded if p != UNCHANGED_PROBLEM]:
            # a recheck that finds what the cleared exception recorded: no
            # fix round (the owner's ruling), the exception again
            return self._exception(batch_id, 'checks', [UNCHANGED_PROBLEM] + problems)
        if problems:
            self.log('%s: %d problem(s); one fix round' % (batch_id, len(problems)))
            res = self._fix(batch_id, batch, worker, worktree, 'mechanical checks',
                            problems, brief, 'worker-fix-checks')
            out = self._settle(batch_id, res, 'worker-fix-checks', worktree)
            if out:
                return out
            verdicts, problems = self._check(batch_id, worktree, res.report)
        if self.dry_run:
            self._ledger_for(batch_id, self._merged(batch_id, verdicts), (
                False, 'dry run: no review, nothing recorded, no close'))
            if problems:
                return self._exception(batch_id, 'checks', problems)
            self._finish(batch_id, 'checked')
            return 'checked'
        if problems:
            return self._exception(batch_id, 'checks', problems)

        # The review, when it can matter; one fix round.
        merged = self._merged(batch_id, verdicts)
        needed, why = self.review_needed(batch_id, merged, self.seed)
        if not needed:
            review = (False, 'not sampled (seed %s)' % self.seed)
        elif self._reviewer_of(name) is None:
            return self._exception(batch_id, 'review', [
                'review needed (%s) but no reviewer is configured' % why])
        else:
            findings, out = self._review(batch_id, batch, worktree, merged, why,
                                         'reviewer')
            if out:
                return out
            fixed = bool(findings)
            if findings:
                res = self._fix(batch_id, batch, worker, worktree, 'review',
                                findings, brief, 'worker-fix-review')
                out = self._settle(batch_id, res, 'worker-fix-review', worktree)
                if out:
                    return out
                verdicts, problems = self._check(batch_id, worktree, res.report)
                if problems:
                    return self._exception(batch_id, 'checks', problems)
                merged = self._merged(batch_id, verdicts)
                findings, out = self._review(batch_id, batch, worktree, merged,
                                             why, 'reviewer-2')
                if out:
                    return out
                if findings:
                    return self._exception(batch_id, 'review', findings)
            review = (True, '%s (seed %s): clean%s' % (
                why, self.seed, ' after one fix round' if fixed else ''))

        return self._land(batch_id, batch, worktree, verdicts, review)

    def _recheck(self, batch_id, batch, worktree, name, recorded):
        """A cleared checks or review exception (Task 25): the last saved
        worker report is checked against the CURRENT worktree (the
        coordinator may have fixed the cause by hand) and the batch goes on as
        after a worker stage.  The worker is not run, the resume preamble is
        not used, and there is no extra fix round for problems the exception
        already recorded."""
        line = 'driver %s: %s' % (batch_id, RECHECK_LEDGER)
        self._append_ledger(line)
        self.log(line)
        self._stage(batch_id, 'recheck')
        report = next(self._saved_reports(batch_id), None)
        if report is None:
            return self._exception(batch_id, 'checks', [
                'no saved worker report to recheck in %s; clear with --rerun' % os.path.join(
                    self.driver_dir, batch_id)])
        if self.kind == 'verdicts':
            self._probe = {k: ('ours', 'probe-%s-%s' % (batch_id, k.upper()))
                           for k in (self._carry_probe_passes(batch_id, worktree) or ())}
        try:
            rc, brief = self.render_brief(
                self.template_path, self.state_path, batch_id,
                bundle_dir=os.path.join(self.driver_dir, 'bundles', batch_id),
                repo=self.repo, base=self.base)
        except Exception as e:
            rc, brief = 1, 'render_brief raised %r' % (e,)
        if rc != 0:
            return self._exception(batch_id, 'brief', [brief.strip()])
        return self._after_worker(batch_id, batch, worktree, name,
                                  self._worker_of(name), brief, report, recorded or [])

    def _saved_verdicts(self, batch_id, batch):
        """The verdicts of the last saved report that the state does not
        hold yet (the retry of a record exception)."""
        key = _tool('wave')._addr_key
        have = {key(a) for a in batch.get('verdicts', {})}
        for text in self._saved_reports(batch_id):
            try:
                parsed = parse_report(text)
            except ReportError:
                continue
            return {a: sc for a, sc in parsed.items() if key(a) not in have}
        return {}

    def _saved_reports(self, batch_id):
        """The texts of the saved worker reports, the last one first."""
        folder = os.path.join(self.driver_dir, batch_id)
        for label in ('worker-fix-review', 'worker-fix-checks', 'worker'):
            try:
                with open(os.path.join(folder, label + '.report.md'),
                          encoding='utf-8') as f:
                    yield f.read()
            except OSError:
                continue

    def _saved_undecided(self, batch_id, batch):
        """The ## Undecided rows of the last saved report that are still in
        the batch with no recorded verdict (a retry of record or close,
        ruling R74: the move is idempotent)."""
        key = _tool('wave')._addr_key
        open_rows = {key(r['address']) for r in batch.get('rows', [])} - \
            {key(a) for a in batch.get('verdicts', {})}
        for text in self._saved_reports(batch_id):
            try:
                parse_report(text)
                listed = parse_undecided(text)
            except ReportError:
                continue
            return {a: sc for a, sc in listed.items() if key(a) in open_rows}
        return {}

    def _move_undecided(self, batch_id):
        """Ruling R73: the rows the worker listed under ## Undecided leave
        the batch and join the probe list (docs/wso/probe-list.json, one
        entry per address), the same way triage moves an ours-stub row to
        the import list.  The list is written first: a crash between the
        two writes leaves the row in both places, never in neither.  The
        caller holds the driver lock."""
        wave = _tool('wave')
        if not self._undecided:
            return
        key = wave._addr_key
        state = self.load_state(self.state_path)
        batch = state['batches'][batch_id]
        gone = {key(a): sc for a, sc in self._undecided.items()}
        moved = [r for r in batch['rows'] if key(r['address']) in gone]
        listed, err = wave._load_list(self.probe_path, 'probe list',
                                      missing_ok=True)
        if err:
            raise ValueError(err.strip())
        have = {key(e['address']) for e in listed}
        at = _now()
        listed += [{'address': r['address'], 'file': r['file'],
                    'name': r.get('name'), 'reading': gone[key(r['address'])][0],
                    'reason': gone[key(r['address'])][1], 'batch': batch_id,
                    'source': 'worker', 'at': at}
                   for r in moved if key(r['address']) not in have]
        wave._save_json(self.probe_path, listed)
        batch['rows'] = [r for r in batch['rows'] if key(r['address']) not in gone]
        batch['files'] = wave._files_of(batch['rows'])
        wave.save_state(self.state_path, state)
        self.log('%s: %d undecided row(s) moved to the probe list' % (
            batch_id, len(moved)))

    def _land(self, batch_id, batch, worktree, verdicts, review):
        """Record, close, push, ledger -- and the retry of a cleared record,
        close or push exception, which comes straight here (ruling R71)."""
        # Record, close, push, ledger.  Every state write and git
        # operation here holds the driver lock (ruling R62); nothing
        # inside the locked blocks takes it again.
        self._stage(batch_id, 'record')
        failed = []
        with self._lock:
            try:
                self._move_undecided(batch_id)
            except Exception as e:
                failed.append('moving the undecided rows to the probe list '
                              'raised %r' % (e,))
        if failed:
            return self._exception(batch_id, 'record', failed)
        if self.kind == 'probes':
            try:
                failed = self._record_probe_rows(batch_id, worktree)
            except Exception as e:
                failed = ['recording the probe rows raised %r' % (e,)]
            if failed:
                return self._exception(batch_id, 'record', failed)
        with self._lock:
            for addr, (side, case) in sorted(verdicts.items()):
                measured = self._measured_for(addr, side)
                if measured == 'frida' and self._body_moved(batch_id, worktree, addr):
                    self.log('%s: %s: body changed after the Frida phase; not recorded as '
                             'measured' % (batch_id, addr))
                    measured = None
                extra = {} if measured is None else {'measured': measured}
                if side == 'fork' and self._before_kind(batch_id, addr):
                    # the losing kind is the evidence's before the phase wrote its trace
                    extra['losing_kind'] = self._before_kind(batch_id, addr)
                rc, text = self.record_verdict(self.state_path, self.repo,
                                               batch_id, addr, side, case,
                                               **extra)
                if rc != 0:
                    failed.append(text.strip())
        if failed:
            return self._exception(batch_id, 'record', failed)
        self._stage(batch_id, 'close')
        with self._landing_lock(batch_id):
            with self._lock, self._writing():
                commits = self._commits(batch['branch'])
                # Uncommitted paths were a mechanical-check problem already
                # (ruling R71); nothing is committed on the worker's behalf.
                # Another driver may fast-forward the base while this one waits
                # on the gate lock: on that error, and only after the gate
                # passed, sync and gate once more in place (ruling R64).
                for attempt in (0, 1):
                    sync = self._sync_branch(batch['branch'], worktree)
                    if sync:
                        rc, text = 1, sync
                        break
                    try:
                        why = self._regen_map(worktree)
                    except Exception as e:
                        why = 'raised %r' % (e,)
                    if why:
                        rc, text = 1, 'regenerating docs/MAP.md failed: %s' % why
                        break
                    try:
                        rc, text = self.close_batch(
                            self.state_path, self.repo, batch_id, base=self.base,
                            gate=self.gate, kind=self.kind)
                    except Exception as e:
                        rc, text = 1, 'close_batch raised %r' % (e,)
                    if rc != 0 and attempt == 0 and 'not a fast-forward' in text:
                        self.log('%s: %s moved during the gate; syncing and '
                                 'closing again' % (batch_id, self.base))
                        continue
                    break
            if rc != 0:
                return self._exception(batch_id, 'close', [
                    l.strip() for l in text.splitlines() if l.strip()])
            self.log(text.rstrip())
            with self._lock, self._writing():
                if self.kind in ('verdicts', 'probes'):
                    # The import queue has no verdicts: its state must not
                    # overwrite wave 1's scorecard.  The probe queue's rows were
                    # resolved into wave 1's state; its scorecard gains the
                    # `probes:` line.
                    try:
                        rc, text = self.write_scorecard(
                            self.state_path if self.kind == 'verdicts'
                            else self.wave_state_path, self.report_path, self.triage_path,
                            calibration_path=self.calibration_path,
                            **({'repo': self.repo} if self.kind == 'probes' else {}))
                        self.log(text.rstrip())
                    except Exception as e:   # the batch is closed; the scorecard can wait
                        self.log('scorecard not written: %r' % (e,))
                state_err = self._commit_state(batch_id)
                err = self.push()
        batch = self.load_state(self.state_path)['batches'][batch_id]
        if self.kind == 'probes':
            # the decided rows are verdicts of the original batches: fork, on a frida case
            batch_verdicts = {r['address']: ('fork', r.get('case', ''))
                              for r in batch['rows'] if r.get('outcome') == 'resolved'}
        else:
            batch_verdicts = batch['verdicts']
        entry = ledger_entry(
            dict(batch, id=batch_id), batch_verdicts,
            self._classes(batch_id, batch), review, commits, self._credits(),
            ignored=self._ignored, undecided=self._undecided,
            gate=batch.get('gate'), **self._figure_kw())
        if self.kind == 'probes':
            entry += self._probe_ledger_lines(batch)
        kept = [(side, a, d) for a, (side, _c) in sorted(verdicts.items())
                for st, d in [self._frida_got.get(_tool('wave')._addr_key(a), ('', ''))]
                if st == 'different']
        entry += ''.join('- frida different, kept %s: %s %s\n' % skd for skd in kept)
        # Frequency-only differences (both kinds): a caller finding for the
        # ledger, never a verdict change or a park.
        entry += ''.join('- frida frequency only (a caller finding): 0x%s %s\n' % (k.upper(), n)
                         for k, notes in sorted(self._frida_notes.items()) for n in notes)
        self._append_ledger(entry)
        self._finish(batch_id, 'closed')
        if err:
            return self._exception(batch_id, 'push', self._push_problems(err, state_err))
        if state_err:
            return self._state_failed(batch_id, state_err)
        return 'closed'

    # -- the loop: cuts and retries -------------------------------------------

    def _recompare(self):
        """Once per run, when the wave-1 list is exhausted: look at Will's
        tip.  Logs `recompare: N new rows at <tip7>`; returns N (0 when
        the comparison failed, which only costs the exit the run had)."""
        if self._recompared:
            return 0
        self._recompared = True
        try:
            with self._lock, self._writing():
                rc, text = self.recompare(
                    self.state_path, self.disagreements_path, repo=self.repo,
                    triage_path=self.triage_path)
        except Exception as e:
            rc, text = 2, 'recompare raised %r\n' % (e,)
        self.log(text.rstrip())
        m = re.match(r'recompare: (\d+) new rows', text)
        return int(m.group(1)) if rc == 0 and m else 0

    def _log_pending_coverage(self):
        """Task 27: one line when `wave.py recompare` left candidates for a
        coverage pass; the driver never launches that pass itself."""
        path = os.path.join(os.path.dirname(self.disagreements_path),
                            'coverage.pending.json')
        try:
            with open(path, encoding='utf-8') as f:
                n = len(json.load(f))
        except (OSError, ValueError, TypeError):
            return
        if n:
            self.log('coverage: %d candidate(s) pending (run `python3 -I '
                     'tools/wave.py coverage --pending`); their batches run '
                     'every scenario until then' % n)

    def _base_key(self):
        """What the last green run is keyed on: the base's HEAD sha and the
        working-tree listing of tests/ and tools/ (status lines, with the
        size and mtime of each listed file).  None when git cannot say."""
        def git(*args):
            return subprocess.run(['git', '-C', self.repo, *args],
                                  capture_output=True, text=True, timeout=120)
        try:
            head = git('rev-parse', 'HEAD')
            st = git('status', '--porcelain', '--untracked-files=all',
                     '--', 'tests', 'tools')
        except (OSError, subprocess.SubprocessError):
            return None
        if head.returncode != 0 or st.returncode != 0:
            return None
        parts = [head.stdout.strip()]
        for line in sorted(st.stdout.splitlines()):
            path = line[3:].split(' -> ')[-1].strip('"')
            try:
                info = os.stat(os.path.join(self.repo, path))
                stamp = '%d:%d' % (info.st_size, info.st_mtime_ns)
            except OSError:
                stamp = 'gone'
            parts.append('%s %s' % (line, stamp))
        return hashlib.sha256('\n'.join(parts).encode()).hexdigest()

    def _base_ok(self):
        """False while the base's tool tests are red (new cuts stop), True
        when they pass or the check is off.  An open exceptions['first']
        blocks without a re-run (`--clear first` lifts it); the suite is
        not re-run while the key of the last green run is unchanged."""
        if not self.base_tests or self.dry_run or self._stopping:
            return True
        status = self._read_status()
        if BASE_TESTS_KEY in status['exceptions']:
            return False
        key = self._base_key()
        last = status.get('base_tests')
        if key is not None and isinstance(last, dict) and last.get('key') == key:
            return True
        log_name = BASE_TESTS_LOG % self.queue
        log_path = os.path.join(self.driver_dir, log_name)
        self.log('base: running the tool tests on %s' % self.base)
        ok, failures = self.base_tests_run(self.repo, log_path)
        if ok:
            green = {'key': key, 'at': _now()}
            self._update_status(lambda s: s.__setitem__('base_tests', green))
            return True
        at = _now()
        what = (failures if isinstance(failures, str) else
                '%d failure%s' % (failures, '' if failures == 1 else 's'))
        msg = ('base: tool tests fail on first (%s; see driver/%s); '
               'no new batch starts until they pass' % (what, log_name))
        exc = {'stage': 'base-tests', 'problems': [msg], 'at': at}

        def record(s):
            s['exceptions'][BASE_TESTS_KEY] = exc
            s.pop('base_tests', None)
        self._update_status(record)
        line = 'driver first: exception at base-tests (%s): %s' % (at, msg)
        self._append_ledger(line)
        self.log(line)
        if not self.dry_run:
            try:
                self.send('driver: first exception at base-tests; see %s'
                          % self.status_path)
            except Exception as e:
                self.log('send failed: %r' % (e,))
        return False

    def _cut(self):
        """(batch_id, None) for a batch cut and triaged, else (None, exit)."""
        self._log_pending_coverage()
        triage = (self.triage_path
                  if _tool('wave').QUEUES[self.queue]['triage'] else None)
        try:
            with self._lock, self._writing():
                rc, text = self.next_batch(
                    self.disagreements_path, self.state_path, self.size,
                    self.category, triage_path=triage, repo=self.repo,
                    base=self.base, queue=self.queue)
        except Exception as e:
            rc, text = 1, 'next_batch raised %r\n' % (e,)
        if (rc == 2 and text.startswith('nothing left:')
                and self.queue == 'wave1' and not self.dry_run):
            n = self._recompare()
            if n:
                return self._cut()
        self.log(text.rstrip())
        m = re.match(r'(\S+) assigned:', text)
        if m:
            batch_id = m.group(1)
            self._started[batch_id] = self._clock()
            t = re.search(r'^triage: \d+ auto, (\d+) import, \d+ ambiguous$',
                          text, re.M)
            if t:
                self._imported[batch_id] = int(t.group(1))
            if rc != 0:
                self.log('%s: triage exited %s; the rows it could not decide '
                         'stay for the worker' % (batch_id, rc))
            self._update_status(lambda s: s.__setitem__('stalled', None))
            return batch_id, None
        if rc == 2 and text.startswith('nothing left to assign'):
            # Rows remain, but only on files that assigned batches own.
            stalled = {'at': _now(), 'message': text.strip()}
            self._update_status(lambda s: s.__setitem__('stalled', stalled))
            return None, 4
        if rc == 2 and text.startswith('nothing left'):
            return None, 0
        m = re.match(r'(\S+) not assigned:', text)
        self._exception(m.group(1) if m else 'next', 'cut',
                        [l for l in text.splitlines() if l.strip()]
                        or ['next_batch exited %s' % rc])
        return None, 1

    def _adoptable(self, taken):
        """Ids of the batches to adopt (ruling R69), oldest assigned_at
        first: assigned, no verdict recorded, and claimed by nothing
        (`taken`: this run's resumes and retries; the status file's
        running, parked and exception entries).  Wave 1 only."""
        if self.dry_run or self.queue != 'wave1':
            return []
        status = self._read_status()
        claimed = set(taken) | set(status['running']) | set(status['exceptions'])
        for holder in [status] + [p for p in status['pools'].values()
                                  if isinstance(p, dict)]:
            claimed.update(parked_map(holder.get('parked')))
        if isinstance(status['current'], dict) and status['current'].get('batch'):
            claimed.add(status['current']['batch'])
        batches = self.load_state(self.state_path)['batches']
        free = [(b.get('assigned_at') or '', bid) for bid, b in batches.items()
                if b.get('status') == 'assigned' and not b.get('verdicts')
                and bid not in claimed]
        return [bid for _at, bid in sorted(free)]

    def _adopt(self, batch_id):
        """None when batch_id is cut (branch and worktree, as next_batch
        does under R14) and triaged like a fresh one, else an exit code."""
        wave = _tool('wave')
        batch = self.load_state(self.state_path)['batches'][batch_id]
        claims = os.path.join(os.path.dirname(os.path.abspath(self.state_path)),
                              'claims.json')
        try:
            with self._lock, self._writing(), \
                    wave._filelock().hold(claims + '.lock', 'wave adopt'):
                wt, how = self.cut_worktree(self.repo, batch['branch'],
                                            self.base)
                if wt is None:
                    rc, text = 1, '%s not adopted: %s\n' % (batch_id, how)
                else:
                    state = self.load_state(self.state_path)
                    rc, text = self.apply_triage(
                        self.state_path, self.repo, batch_id, self.triage_path,
                        state.get('fork_base') or wave.DEFAULT_FORK_BASE)
                    text = ('%s adopted: worktree %s (%s)\n' % (
                        batch_id, wt, how)) + text
        except Exception as e:
            rc, text = 1, '%s not adopted: adoption raised %r\n' % (batch_id, e)
        self.log(text.rstrip())
        if text.startswith(batch_id + ' adopted:'):
            self._started[batch_id] = self._clock()
            t = re.search(r'^triage: \d+ auto, (\d+) import, \d+ ambiguous$',
                          text, re.M)
            if t:
                self._imported[batch_id] = int(t.group(1))
            if rc != 0:
                self.log('%s: triage exited %s; the rows it could not decide '
                         'stay for the worker' % (batch_id, rc))
            self._update_status(lambda s: s.__setitem__('stalled', None))
            return None
        self._exception(batch_id, 'cut', [l for l in text.splitlines()
                                          if l.strip()])
        return 1

    def _retry_why(self, batch_id):
        """The resume text for a cleared batch (None: it goes straight to
        its stage, or is closed), or False when it has no worktree and no
        verdict and so is adopted anew (ruling R71)."""
        batch = self.load_state(self.state_path)['batches'].get(batch_id) or {}
        stage = self._retry_stage.get(batch_id)
        if batch.get('status') != 'assigned' or stage in ('record', 'close', 'recheck'):
            return None
        if not batch.get('verdicts') and self.queue == 'wave1' and \
                not self.dry_run and (self.worktree_for(
                    self.repo, batch.get('branch', '')) is None):
            return False
        return 'had its exception at the %s stage cleared: the work so ' \
               'far is on its branch' % (stage or 'earlier')

    def _cleared(self):
        """Batches whose exception the coordinator cleared on disk: retried
        first."""
        state = self.load_state(self.state_path)
        status = self._read_status()
        out = []
        for bid, info in sorted(status['batches'].items()):
            if not (isinstance(info, dict) and info.get('outcome') == 'exception'
                    and bid not in status['exceptions']):
                continue
            st = state['batches'].get(bid, {}).get('status')
            stage = info.get('stage')
            if st == 'assigned' or (st == 'closed' and stage in ('push', 'state')):
                out.append(bid)
                note = (status.get('cleared') or {}).get(bid)
                note = note if isinstance(note, dict) else {}
                if stage in ('record', 'close') and note.get('rerun_frida'):
                    # --rerun-frida was on at --clear time: that batch's
                    # retry re-runs its Frida phase (ruling R71 without it)
                    self._retry_frida[bid] = True
                if stage in RECHECK_STAGES and note:
                    # plain --clear re-checks the last report; --rerun, or an
                    # exception removed by hand (no note), re-enters at the
                    # worker
                    kind = 'rerun' if note.get('rerun') else 'recheck'
                    self._retry_kind[bid] = kind
                    self._retry_problems[bid] = [str(p) for p in note.get('problems') or []]
                    if kind == 'recheck':
                        stage = 'recheck'
                self._retry_stage[bid] = stage
        return out

    # -- the budget (ruling R38) ----------------------------------------------

    @property
    def estimate_next(self):
        """The next batch's credits: the mean of this run's batches with
        known credits, DEFAULT_ESTIMATE before there is one."""
        known = [c for c in self._batch_credits if c is not None]
        return sum(known) / len(known) if known else DEFAULT_ESTIMATE

    @property
    def credits_spent(self):
        """This run's credits; a batch with unknown credits counts as the
        estimate (unknown is not free)."""
        est = self.estimate_next
        return sum(est if c is None else c for c in self._batch_credits)

    def _over_budget(self):
        """True, with status['stopped'] written, when the credits spent
        plus the next batch's estimate pass the budget.  Checked before
        each new batch, never mid-batch."""
        if self.budget is None:
            return False
        spent, est = self.credits_spent, self.estimate_next
        if spent + est <= self.budget:
            return False
        stopped = {'at': _now(), 'reason': 'budget', 'spent': spent,
                   'budget': self.budget}
        self._update_status(lambda s: s.__setitem__('stopped', stopped))
        self.log('budget stop: %.0f credits spent + %.0f estimated for the '
                 'next batch > budget %.0f' % (spent, est, self.budget))
        return True

    # -- what the last run left (resume) --------------------------------------

    def _has_ledger_entry(self, batch_id):
        head = '## driver %s (' % batch_id
        try:
            with open(self.ledger_path, encoding='utf-8') as f:
                return any(line.startswith(head) for line in f)
        except FileNotFoundError:
            return False

    def _settle_closed(self, batch_id):
        """A batch the last run closed and then stopped on: its ledger
        entry when it is missing, the push, then the status.  1 when the
        push fails (the push exception) or the state commit does (the state
        exception), else None."""
        self._runs = []
        self._ignored = {}
        batch = self.load_state(self.state_path)['batches'][batch_id]
        if self._has_ledger_entry(batch_id):
            self.log('%s: closed by the last run, ledger entry present; '
                     'pushing again' % batch_id)
        else:
            folder = os.path.join(self.driver_dir, batch_id)
            reviewed = any(os.path.isfile(os.path.join(folder, l + '.report.md'))
                           for l in ('reviewer', 'reviewer-2'))
            tip = self._git('rev-parse', '--verify', batch['branch']).stdout.strip()
            self._append_ledger(ledger_entry(
                dict(batch, id=batch_id), batch.get('verdicts', {}),
                self._classes(batch_id, batch),
                (reviewed, 'recovered after the last run stopped between '
                           'close and ledger'),
                'unknown..%s (recovered)' % (tip[:8] or batch['branch']), None,
                pool=batch.get('pool'), gate=batch.get('gate')))
            self.log('%s: closed by the last run without its ledger entry; '
                     'entry written' % batch_id)
        with self._landing_lock(batch_id), self._lock:
            state_err = self._commit_state(batch_id)
            err = self.push()
        self._finish(batch_id, 'closed')
        if err:
            self._exception(batch_id, 'push', self._push_problems(err, state_err))
            return 1
        if state_err:
            self._state_failed(batch_id, state_err)
            return 1
        return None

    def _pool_or_first(self, name):
        return name if name in self._busy else self.pools[0].name

    def _leftovers(self):
        """What the last run left behind: ([(batch_id, why, pool)] to
        resume before anything new, None), or ([], exit code).  First a
        batch it closed but did not finish (current or running names a
        closed batch), then each pool's parked batch (a top-level `parked`
        from before the pools is moved to its batch's pool once), then a
        batch it left in flight (the process died mid-batch)."""
        status = self._read_status()
        batches = self.load_state(self.state_path)['batches']
        old = parked_map(status['parked'])
        if old:
            def migrate(s):
                for bid, info in parked_map(s['parked']).items():
                    owner = self._pool_or_first(
                        (batches.get(bid) or {}).get('pool'))
                    pool = s['pools'].setdefault(owner, {})
                    m = parked_map(pool.get('parked'))
                    m.setdefault(bid, info)
                    _set_parked(pool, m)
                s['parked'] = None
            status = self._update_status(migrate)
        current = status['current'] if isinstance(status['current'], dict) else {}
        flying = {}
        for bid, info in status['running'].items():
            if isinstance(info, dict):
                flying[bid] = dict(info)
        if current.get('batch'):
            flying.setdefault(current['batch'], current)
        for cur in sorted(flying):
            if batches.get(cur, {}).get('status') == 'closed':
                if self._settle_closed(cur):
                    return [], 1
                flying.pop(cur)
        resume = []
        parked_ids = set()
        for name, info in sorted(status['pools'].items()):
            if not isinstance(info, dict):
                continue
            for pid, parked in sorted(parked_map(info.get('parked')).items()):
                if pid in parked_ids:   # named by two pools: resumed once
                    continue
                parked_ids.add(pid)
                st = batches.get(pid, {}).get('status')
                if st == 'assigned':
                    named = (batches[pid].get('pool')
                             if (batches[pid].get('pool') in status['pools']
                                 and pid in parked_map(status['pools'][
                                     batches[pid]['pool']].get('parked')))
                             else name)
                    resume.append((pid, 'was parked by the driver at %s (%s)' % (
                        parked.get('at'), parked.get('reason')),
                        self._pool_or_first(named)))
                else:
                    self._update_status(lambda s, pid=pid: _unpark_everywhere(s, pid))
                    self.log('%s was parked but is %s; parked cleared' % (
                        pid, st or 'not in the wave state'))
        for cur, info in sorted(flying.items()):
            if cur in parked_ids:
                continue
            st = batches.get(cur, {}).get('status')
            if st == 'assigned':
                resume.append((cur, 'was interrupted at its %s stage (%s): '
                                    'the last run stopped mid-batch' % (
                                        info.get('stage'), info.get('at')),
                               self._pool_or_first(
                                   batches[cur].get('pool') or info.get('pool'))))
            else:
                def clear(s, cur=cur):
                    s['running'].pop(cur, None)
                    if isinstance(s['current'], dict) and \
                            s['current'].get('batch') == cur:
                        s['current'] = None
                self._update_status(clear)
                self.log('%s was in flight but is %s; current cleared' % (
                    cur, st or 'not in the wave state'))
        return resume, None

    # -- pools ----------------------------------------------------------------

    def _pool_estimate(self, name):
        known = [c for c in self._pool_batch_credits[name] if c is not None]
        return sum(known) / len(known) if known else DEFAULT_ESTIMATE

    def _pool_spent(self, name):
        est = self._pool_estimate(name)
        return sum(est if c is None else c
                   for c in self._pool_batch_credits[name])

    def _init_pools(self):
        """status['pools'][name]: this run's credits (0), the budget, no
        stop; an earlier park stays until its batch is resumed."""
        def init(s):
            for p in self.pools:
                old = s['pools'].get(p.name)
                parked = (parked_map(old.get('parked')) or None
                          if isinstance(old, dict) else None)
                s['pools'][p.name] = {
                    'credits': 0.0, 'budget': self.budgets.get(p.name),
                    'parked': parked, 'stopped': None}
        self._update_status(init)

    def _pool_over_budget(self, name, status):
        """True, with the pool's `stopped: 'budget'` written, when its
        credits plus the next batch's estimate pass its budget."""
        budget = self.budgets.get(name)
        if budget is None:
            return False
        info = status['pools'].get(name) or {}
        if info.get('stopped') == 'budget':
            return True
        spent, est = self._pool_spent(name), self._pool_estimate(name)
        if spent + est <= budget:
            return False
        self._update_status(lambda s: s['pools'].setdefault(name, {})
                            .__setitem__('stopped', 'budget'))
        self.log('pool %s budget stop: %.0f credits spent + %.0f estimated '
                 'for the next batch > budget %.0f' % (name, spent, est, budget))
        return True

    def pick_pool(self):
        """The pool for the next batch, or None: the first, from a
        round-robin cursor that advances after each assignment, that has a
        free worker, is not parked and is under its budget.  Call under
        the lock."""
        status = self._read_status()
        ok = []
        for i, p in enumerate(self.pools):
            info = status['pools'].get(p.name)
            if self._busy[p.name] >= p.workers:
                continue
            if p.name in self._parked_now or (
                    isinstance(info, dict) and parked_map(info.get('parked'))):
                continue
            if self._pool_over_budget(p.name, status):
                continue
            ok.append(i)
        if not ok:
            return None
        n = len(self.pools)
        for off in range(n):
            i = (self._cursor + off) % n
            if i in ok:
                self._cursor = (i + 1) % n
                return self.pools[i].name

    # -- the loop -------------------------------------------------------------

    def _halt_run(self, reason):
        """Stop the run (exit 1) with status['halted'] = {at, reason}, and
        say so once on the AoE session.  A later run that starts a batch
        clears it."""
        self._halt = True
        halted = {'at': _now(), 'reason': reason}
        self._update_status(lambda s: s.__setitem__('halted', halted))
        self.log('driver halted: %s' % reason)
        if not self.dry_run:
            try:
                self.send(_one_line('iwd2r driver halted: %s (status %s)' % (
                    reason, self.status_path), 400))
            except Exception as e:   # a failed send must not hide the halt
                self.log('send failed: %r' % (e,))

    def _one(self, batch_id, resume=None, pool=None):
        """Run one batch on `pool`, count its
        credits, and feed the breaker; an exit code when the run must
        stop, else None."""
        self._runs = []
        self._worker_ran = []
        self._ctx.pool = pool
        try:
            out = (self.run_batch(batch_id, resume=resume) if resume
                   else self.run_batch(batch_id))
        finally:
            # Nothing of this batch's figures may reach the next exception
            # this thread writes (a cut failure has no batch).
            self._ctx.t0 = None
            self._ctx.final = None
        name = self._pool_name()
        if self._runs:
            credits = self._credits()
            with self._lock:
                self._batch_credits.append(credits)
                if name in self._pool_batch_credits:
                    self._pool_batch_credits[name].append(credits)
                    spent = self._pool_spent(name)
                    self._update_status(lambda s: s['pools'].setdefault(
                        name, {}).__setitem__('credits', spent))
        if self._halt:
            return 1
        if out == 'parked':
            with self._lock:
                self._parked_now.add(name)   # whatever the status file says
            return 3
        # The breaker (rulings R45, R46): a batch exception whose worker
        # never really ran (no transcript, or one with no successful
        # non-synthetic turn) counts; any other batch resets.  Credits are
        # no signal: a Claude pool's read 0.  A batch that launched no
        # worker this run (a recheck, a record or close retry) is excluded
        # from the count either way (Task 33): its exception has no
        # transcript by design, so it is no evidence and no counter-signal.
        tripped = False
        with self._lock:
            if out == 'exception' and self._worker_ran \
                    and not any(self._worker_ran):
                self._no_transcript += 1
                tripped = self._no_transcript >= BREAKER_LIMIT
            elif self._worker_ran:
                self._no_transcript = 0
        if tripped:
            self._halt_run('breaker: %d batches without a worker transcript'
                           % BREAKER_LIMIT)
            return 1
        return None

    def _take_run_lock(self):
        """(fd, None) holding a non-blocking flock on
        `<status_path>.run.lock` with this pid written into it, or (None,
        the holder's pid) when another driver has it (ruling R42)."""
        path = self.status_path + '.run.lock'
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        fd = os.open(path, os.O_RDWR | os.O_CREAT, 0o644)
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            holder = os.pread(fd, 32, 0).decode('ascii', 'replace').strip()
            os.close(fd)
            return None, holder or 'unknown'
        except BaseException:
            os.close(fd)
            raise
        os.ftruncate(fd, 0)
        os.pwrite(fd, b'%d\n' % os.getpid(), 0)
        return fd, None

    def run(self):
        """0 when every row is assigned or decided, or at the budget stop;
        3 when a pool ended parked by a rate limit; 4 when stalled (the
        rows left wait on files owned by assigned batches); 1 when an
        exception that is not the batch's own stopped the run, the run
        halted (a launch failure, an auth or billing API error, the
        breaker: status['halted']), the status file cannot be read, or
        another driver runs on the same status file.  What the last run
        left goes first: a closed batch's missing ledger entry and push,
        then each pool's parked batch on that pool, then an interrupted
        one.  A dry run processes one batch and exits 0."""
        fd, holder = self._take_run_lock()
        if fd is None:
            self.log('another driver is running (pid %s) on %s; this one '
                     'stops' % (holder, self.status_path))
            return 1
        self._stopping = False
        self._stop_deferred = None
        _STOPPED.clear()
        previous = {}
        for sig in (signal.SIGTERM, signal.SIGINT):
            try:
                previous[sig] = signal.signal(sig, self._on_stop)
            except ValueError:     # not the main thread: no handler
                pass
        try:
            return self._loop()
        except _Stop:
            self.log('driver stopped by a signal: its workers are killed; '
                     'the batches in flight stay recorded for resume')
            return 1
        except StatusError as e:
            self.log('driver stopped: the status file cannot be read: %s. '
                     'Repair it (one JSON object), then run again.' % e)
            return 1
        finally:
            for sig, handler in previous.items():
                signal.signal(sig, handler)
            _STOPPED.clear()
            try:
                os.ftruncate(fd, 0)
            except OSError:
                pass
            os.close(fd)   # releases the run lock

    def _loop(self):
        self._halt = False            # per run, like the counters below
        if self.dry_run:
            batch_id, code = self._cut()
            if batch_id is None:
                return code
            self.run_batch(batch_id)
            self.log('dry run: %s left assigned; ledger and status in %s'
                     % (batch_id, self.driver_dir))
            return 0
        self._batch_credits = []
        self._pool_batch_credits = {p.name: [] for p in self.pools}
        self._no_transcript = 0
        self._busy = {p.name: 0 for p in self.pools}
        self._parked_now = set()
        self._began = 0
        self._init_pools()
        if self._read_status()['halted'] is not None:
            self._update_status(lambda s: s.__setitem__('halted', None))
        if self._read_status()['stopped'] is not None:
            self._update_status(lambda s: s.__setitem__('stopped', None))
        self._base_ok()   # red: exceptions['first'], and no cut below
        resume, code = self._leftovers()
        if code is not None:
            return code
        return self._schedule(resume, self._cleared())

    def _start(self, fn):
        """Run fn on its own thread (the default), or as the injected
        runner says; the synchronous runner `lambda fn: fn()` keeps a
        test deterministic."""
        if self._stopping:
            raise _Stop()
        if self._runner is not None:
            self._runner(fn)
        elif sum(p.workers for p in self.pools) <= 1:
            fn()   # one worker: no second thread to overlap with
        else:
            t = threading.Thread(target=fn, name='driver-batch')
            self._threads.append(t)
            t.start()

    def _schedule(self, resume, retries):
        """Hand batches to the pools until nothing is left: resumes first
        (each on its own pool), then the cleared exceptions, then new
        cuts.  One thread per busy worker; this thread only assigns, under
        the lock, and collects the finished ones."""
        done = queue_mod.Queue()
        self._log_pending_coverage()
        pending = list(resume)
        # A batch in flight at the last stop may also read as a cleared
        # exception (its status entry keeps outcome: exception after a
        # --clear); it resumes once, never also as a retry (b019, 2026-10-23).
        in_flight = {b for b, _w, _n in pending}
        retries = [b for b in retries if b not in in_flight]
        running = 0
        exhausted = None      # the exit code once a cut finds nothing
        budget_stop = False   # the budget, not a verdict, ended the run
        cut_blocked = False   # stalled while batches run: retry after one ends
        failure = None        # 1 when the run must stop
        adopt = self._adoptable([b for b, _w, _n in pending] + retries)
        capped = False        # --max-batches reached
        crash = None          # an exception that escaped a batch thread
        base_blocked = False  # the base's tool tests are red: no new cut
        launched = set()      # every batch this run started (no second pickup)

        threads = self._threads = []

        def body(batch_id, why, name, resumed):
            err = None
            try:
                code = self._one(batch_id, why, name)
            except BaseException as e:   # reported by the scheduler
                code, err = 1, e
            finally:
                with self._lock:
                    self._busy[name] -= 1
            done.put((batch_id, code, err, resumed))

        def launch(batch_id, why, name, resumed=False):
            nonlocal running
            running += 1
            self._began += 1
            launched.add(batch_id)
            self._start(lambda: body(batch_id, why, name, resumed))

        def reap(block):
            nonlocal running, cut_blocked, failure, crash, exhausted
            try:
                batch_id, code, err, resumed = done.get(block=block)
            except queue_mod.Empty:
                return False
            running -= 1
            cut_blocked = False
            if err is not None and crash is None:
                crash = err
            if code == 1:
                failure = 1
            if (resumed and exhausted is not None and not budget_stop
                    and code != 1 and err is None):
                # A retried or resumed batch holds files that keep rows out
                # of the cut's eligible set until it finishes; an exhaustion
                # verdict formed while it ran (run 24, 2026-10-25) never saw
                # those rows freed, so one more cut is tried before the run
                # exits on it.
                self.log('%s finished after the pool was judged exhausted; '
                         'trying one more cut' % batch_id)
                exhausted = None
            return True

        def launch_next():
            nonlocal exhausted, cut_blocked, failure, capped, base_blocked, budget_stop
            if self._stopping:
                return False
            if self._halt:
                return False
            if self.max_batches is not None and self._began >= self.max_batches:
                if exhausted is None and not capped:
                    capped = True
                    stopped = {'at': _now(), 'reason': 'max-batches',
                               'n': self.max_batches}
                    self._update_status(lambda s: s.__setitem__('stopped', stopped))
                    self.log('--max-batches %d: %d started; no new batch '
                             'starts' % (self.max_batches, self._began))
                return False
            if pending:
                with self._lock:
                    for i, (batch_id, why, name) in enumerate(pending):
                        pool = next(p for p in self.pools if p.name == name)
                        if self._busy[name] < pool.workers:
                            pending.pop(i)
                            self._busy[name] += 1
                            break
                    else:
                        batch_id = None
                if batch_id is not None:
                    # A resumed batch is in flight again, not parked: off
                    # every pool's map (ruling R64).
                    self._update_status(lambda s: _unpark_everywhere(s, batch_id))
                    launch(batch_id, why, name, resumed=True)
                    return True
                # Its pool is busy: the idle pools go on with new cuts.
            if exhausted is not None or cut_blocked:
                return False
            if self._over_budget():
                budget_stop = True
                exhausted = 0
                return False
            with self._lock:
                name = self.pick_pool()
                if name is not None:
                    self._busy[name] += 1
            if name is None:
                return False
            if not retries:
                # An exception cleared while the driver runs (`wave.py run
                # --clear`): outside the lock, as at the start.
                live = set(self._read_status()['running'])
                skip = launched | live | {b for b, _w, _n in pending}
                for bid in self._cleared():
                    if bid not in skip:
                        retries.append(bid)
                        self.log('%s: exception cleared while running; '
                                 'retrying at its %s stage'
                                 % (bid, self._retry_stage.get(bid)))
            if retries:
                bid = retries.pop(0)
                why = self._retry_why(bid)
                if why is False:           # a cut that never happened: adopt it
                    code = self._adopt(bid)
                    if code is not None:
                        with self._lock:
                            self._busy[name] -= 1
                        failure = code
                        return False
                    self._record_pool(bid, name)
                    why = None
                launch(bid, why, name, resumed=True)
                return True
            if adopt:
                batch_id = adopt.pop(0)
                code = self._adopt(batch_id)
                if code is not None:
                    with self._lock:
                        self._busy[name] -= 1
                    failure = code
                    return False
                self._record_pool(batch_id, name)
                launch(batch_id, None, name)
                return True
            if not self._base_ok():
                with self._lock:
                    self._busy[name] -= 1
                base_blocked = True
                return False
            base_blocked = False
            batch_id, code = self._cut()
            if batch_id is None:
                with self._lock:
                    self._busy[name] -= 1
                if code == 4 and running:
                    cut_blocked = True
                elif code == 1:
                    failure = 1
                else:
                    exhausted = code
                return False
            launch(batch_id, None, name)
            return True

        try:
            while True:
                while reap(False):
                    pass
                if failure is None and crash is None and launch_next():
                    continue
                if running == 0:
                    break
                reap(True)
        finally:
            for t in threads:   # never leave a batch running past the run lock
                t.join()
        if crash is not None:
            raise crash
        if failure:
            return failure
        if base_blocked:
            return 1
        status = self._read_status()
        if self._parked_now or any(
                isinstance(p, dict) and parked_map(p.get('parked'))
                for p in status['pools'].values()):
            return 3
        return exhausted or 0

    # -- the defaults for push and send (ruling R28) ------------------------

    def _git_push(self):
        """None, or why `git push iwd2r first:main first:first` failed."""
        try:
            r = subprocess.run(['git', '-C', self.repo, *PUSH_ARGS],
                               capture_output=True, text=True, timeout=600)
        except (OSError, subprocess.SubprocessError) as e:
            return repr(e)
        if r.returncode != 0:
            return _one_line((r.stderr or '') + (r.stdout or '')) or \
                'exit %s' % r.returncode
        return None

    def _aoe_send(self, line):
        """`aoe send <session> <line>`, only when aoe_session is set."""
        if not self.aoe_session:
            return
        try:
            r = subprocess.run(['aoe', 'send', self.aoe_session, line],
                               capture_output=True, text=True, timeout=60,
                               env=send_env(os.environ))
        except (OSError, subprocess.SubprocessError) as e:
            self.log('aoe send failed: %r' % (e,))
            return
        if r.returncode != 0:
            self.log('aoe send exited %s: %s' % (
                r.returncode, _one_line((r.stderr or '') + (r.stdout or ''))))


def _parse(argv):
    """(args, None), or (None, (exit code, message)) for `wave.py run`.

    `--budget-NAME C` is parsed here by hand: the pool names are only
    known after `--pool` (ruling R12), so argparse cannot declare them."""
    wave = _tool('wave')
    p = argparse.ArgumentParser(
        prog='wave.py run', allow_abbrev=False,
        description='Run the wave batches: cut, triage, worker, checks, '
                    'review, close, push, ledger.')
    p.add_argument('--queue', choices=sorted(wave.QUEUES), default='wave1',
                   help='wave1 (both_changed_differ, b<NNN>), import '
                        '(theirs_real_ours_stub, then theirs_changed_ours_base, '
                        'then only_theirs, '
                        'and the import list, i<NNN>) or probes (the probe '
                        "list's rows, p<NNN>: the fork body adopted, the Frida "
                        'phase decides)')
    p.add_argument('--pool', action='append', default=[], metavar='SPEC',
                   help='NAME=WRAPPER:MODEL[:WORKERS], or the short form '
                        'NAME[:WORKERS] of glm or claude; repeatable '
                        '(default: %s)' % DEFAULT_POOL)
    p.add_argument('--reviewer-model', action='append', default=[],
                   metavar='NAME=MODEL',
                   help="a pool's reviewer model; repeatable (default: "
                        'sonnet for claude, glm-5.3-flash[1m] for glm, else '
                        "the pool's worker model)")
    p.add_argument('--workers', type=int, default=None,
                   help="the default pool's worker count (an error with "
                        '--pool: give each pool its :WORKERS)')
    p.add_argument('--budget', type=float, default=None,
                   help='credits over all pools: stop before a new batch '
                        'would pass it; --budget-NAME C caps one GLM pool')
    p.add_argument('--max-batches', type=int, default=None, metavar='N',
                   help='stop starting batches once N have started in this '
                        'run (adopted, cut or resumed); the ones in flight '
                        'finish, and the run exits 0')
    p.add_argument('--size', type=int, default=None, metavar='N',
                   help='rows per batch (default: %d): a smaller batch keeps '
                        'an import worker inside its timeout' % wave.DEFAULT_SIZE)
    p.add_argument('--worker-timeout', type=float, default=DEFAULT_TIMEOUT,
                   metavar='SECONDS',
                   help='kill a worker or reviewer run (its whole process '
                        'group) after this long and park the batch with the '
                        'reason timeout (default %(default)s)')
    p.add_argument('--seed', default=DEFAULT_SEED,
                   help='the review sample seed (default %(default)s)')
    p.add_argument('--state', help='the queue\'s state file '
                                   '(default: its docs/wso state file)')
    p.add_argument('--aoe-session', help='the AoE session that gets '
                                         'exception lines')
    p.add_argument('--frida', action=argparse.BooleanOptionalAction, default=None,
                   help='run the batched Frida differential path after the worker '
                        '(default: docs/wso/truth-calibration.json frida_default; '
                        'off when that file is missing)')
    p.add_argument('--no-base-tests', dest='base_tests', action='store_false',
                   help='skip the tool tests the driver runs on the base at '
                        'start and before each new cut (tests, dry runs)')
    p.add_argument('--frida-cache', action=argparse.BooleanOptionalAction, default=True,
                   help='reuse the Frida results of an earlier phase of the same batch when '
                        'its tree ids, rows, signatures and scenarios are unchanged '
                        '(--no-frida-cache always runs)')
    p.add_argument('--frida-map',
                   help='override: measure an existing x86 build (the exe beside this '
                        'iwd2r.map, at <checkout>/' + X86_MAP_REL + ') instead of '
                        "building each batch's worktree (hand runs; the default builds "
                        'the batch so the bodies under test are its own)')
    one = p.add_mutually_exclusive_group()
    one.add_argument('--dry-run', action='store_true',
                     help='one batch with a fake worker; no review, no close, '
                          'no push, no aoe; needs --state')
    one.add_argument('--clear', metavar='BATCH_ID',
                     help="remove that batch's exception from the status "
                          "file, under its lock (the coordinator's retry); "
                          "exit 2 when there is none")
    p.add_argument('--rerun', action='store_true',
                   help='with --clear: re-enter a checks or review exception at '
                        'the worker stage (the default re-checks the last report)')
    p.add_argument('--rerun-frida', action='store_true',
                   help='with --clear on the import queue: a record or close '
                        'exception re-runs the batch\'s Frida phase on its '
                        'worktree before it lands (a plain --clear keeps R71: '
                        'straight to the stage, no phase, no worker); without '
                        '--clear it sets the same trigger for that run')
    try:
        args, extra = p.parse_known_args(argv)
    except SystemExit as e:
        return None, (e.code if isinstance(e.code, int) else 2, '')
    args.budgets = {}
    i = 0
    while i < len(extra):
        m = re.fullmatch(r'--budget-([A-Za-z0-9_-]+)(?:=(.*))?', extra[i])
        if not m:
            return None, (2, 'wave.py run: unrecognized argument %s' % extra[i])
        value = m.group(2)
        if value is None:
            i += 1
            value = extra[i] if i < len(extra) else ''
        try:
            args.budgets[m.group(1)] = float(value)
        except ValueError:
            return None, (2, 'wave.py run: --budget-%s needs a number of '
                             'credits, not %r' % (m.group(1), value))
        i += 1
    return args, None


def _pools_of(args):
    """([Pool], None), or (None, message) for the --pool, --workers and
    --reviewer-model flags."""
    if args.pool and args.workers is not None:
        return None, ('--workers sets the default pool\'s worker count and '
                      'cannot be combined with --pool: give each pool its '
                      ':WORKERS')
    if args.workers is not None and args.workers < 1:
        return None, '--workers must be at least 1'
    reviewers = {}
    for spec in args.reviewer_model:
        name, eq, model = spec.partition('=')
        if not eq or not name or not model:
            return None, '--reviewer-model %r: expected NAME=MODEL' % spec
        reviewers[name] = model
    try:
        pools = [parse_pool(spec, args.queue) for spec in (
            args.pool or [DEFAULT_POOL])]
        if args.workers is not None:
            pools[0] = pools[0]._replace(workers=args.workers)
        if len({p.name for p in pools}) != len(pools):
            return None, 'two --pool specs have the same name'
        return pools_with_reviewers(pools, reviewers), None
    except ValueError as e:
        return None, str(e)


def _build(args, repo):
    """(Driver, None), or (None, (exit code, message))."""
    wave = _tool('wave')
    queue = args.queue
    pools, why = _pools_of(args)
    if pools is None:
        return None, (2, 'wave.py run: %s' % why)
    if args.dry_run and not args.state:
        return None, (2, 'wave.py run --dry-run refuses without an explicit '
                         '--state PATH: a dry run cuts a real batch (branch, '
                         'worktree, triage commits) in that state file. Point '
                         'it at a copy, never at docs/wso/wave-state.json.')
    if args.max_batches is not None and args.max_batches < 1:
        return None, (2, 'wave.py run: --max-batches must be at least 1')
    if args.size is not None and args.size < 1:
        return None, (2, 'wave.py run: --size must be at least 1')
    if args.worker_timeout <= 0:
        return None, (2, 'wave.py run: --worker-timeout must be positive')
    live_path = wave.QUEUES[queue]['state']
    if args.dry_run:
        live = {os.path.realpath(live_path), os.path.realpath(
            os.path.join(repo, 'docs', 'wso', os.path.basename(live_path)))}
        if os.path.realpath(args.state) in live:
            return None, (2, 'wave.py run --dry-run refuses %s: that is the '
                             'live %s file. Copy it outside docs/wso '
                             'and pass the copy.' % (
                                 args.state, os.path.basename(live_path)
                                 .replace('.json', '')))
    state = os.path.abspath(args.state) if args.state else live_path
    ledger = os.path.join(repo, LEDGER_REL)
    status = os.path.join(repo, status_rel(queue))
    workers_by_pool = reviewers_by_pool = None
    if args.dry_run:
        dry = os.path.join(os.path.dirname(status), 'dry-run')
        ledger = os.path.join(dry, 'progress.md')
        status = os.path.join(dry, os.path.basename(status))
        worker = reviewer = None
    else:
        worker = reviewer = None
        workers_by_pool = {p.name: HeadlessWorker(model=p.model, wrapper=p.wrapper,
                                           timeout=args.worker_timeout)
                           for p in pools}
        reviewers_by_pool = {p.name: HeadlessWorker(model=p.reviewer_model,
                                                    wrapper=p.wrapper,
                                                    timeout=args.worker_timeout)
                             for p in pools}
    frida = args.frida
    if frida is None:
        frida = _tool('truth').frida_default(
            os.path.join(repo, 'docs', 'wso', 'truth-calibration.json'))
    try:
        d = Driver(repo, state, worker, reviewer, ledger, status,
                   aoe_session=args.aoe_session, seed=args.seed,
                   budget=args.budget, dry_run=args.dry_run, frida=frida,
                   frida_map=args.frida_map, rerun_frida=args.rerun_frida,
                   frida_cache=args.frida_cache, pools=pools,
                   workers_by_pool=workers_by_pool,
                   reviewers_by_pool=reviewers_by_pool,
                   budgets=args.budgets, queue=queue,
                   max_batches=args.max_batches,
                   base_tests=args.base_tests, size=args.size)
    except StatusError as e:
        return None, (1, 'wave.py run: the status file cannot be read: %s' % e)
    except ValueError as e:
        return None, (2, 'wave.py run: %s' % e)
    return d, None


def driver_from_args(argv, repo=None):
    """(Driver, None), or (None, (exit code, message)) for `wave.py run`."""
    args, err = _parse(argv)
    if err:
        return None, err
    return _build(args, repo or _tool('wave').ROOT)


def _clear(batch_id, repo, queue='wave1', rerun=False, rerun_frida=False):
    """`wave.py run --clear <batch_id>`: 0, 2 when the status file holds
    no exception for it, 1 when the file cannot be read."""
    path = os.path.join(repo, status_rel(queue))
    try:
        rc, entry = clear_exception(path, batch_id, rerun, rerun_frida)
    except StatusError as e:
        print('wave.py run --clear: the status file cannot be read: %s' % e,
              file=sys.stderr)
        return 1
    if rc != 0:
        print('wave.py run --clear: no exception for %s in %s' % (batch_id, path),
              file=sys.stderr)
        return rc
    print('removed the exception for %s from %s: %s' % (
        batch_id, path, json.dumps(entry, sort_keys=True)))
    return 0


def remeasure_main(argv=None, repo=None):
    """`wave.py remeasure`: see Driver.remeasure."""
    wave = _tool('wave')
    wave._line_buffer()
    repo = repo or wave.ROOT
    ap = argparse.ArgumentParser(prog='wave.py remeasure', description=(
        'A Frida-only pass over closed wave-1 rows: trace evidence for the ones that measure equal '
        '(on a p3/remeasure-<stamp> branch), docs/wso/engine-gaps.json entries for the ones that differ.'))
    sel = ap.add_mutually_exclusive_group(required=True)
    sel.add_argument('--rows', nargs='+', metavar='ADDR')
    sel.add_argument('--read-only-measured', action='store_true')
    sel.add_argument('--dump-decided', action='store_true')
    ap.add_argument('--batch', default=None)
    ap.add_argument('--chunk', type=int, default=40)
    ap.add_argument('--dry', action='store_true')
    ap.add_argument('--branch', default=None)
    ap.add_argument('--state', default=None, help=argparse.SUPPRESS)
    try:
        args = ap.parse_args(argv)
    except SystemExit as e:
        return 2 if e.code else 0
    if args.chunk < 1 or (args.batch and not args.dump_decided):
        print('wave.py remeasure: --chunk is at least 1; --batch goes with --dump-decided',
              file=sys.stderr)
        return 2
    mode = ('rows' if args.rows else 'read-only-measured' if args.read_only_measured
            else 'dump-decided')
    state = os.path.abspath(args.state) if args.state else os.path.join(repo, 'docs', 'wso', 'wave-state.json')
    picked, skipped = wave.remeasure_select(state, repo, mode, args.rows or (), args.batch)
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    status = os.path.join(repo, DRIVER_REL, 'remeasure-%s' % stamp, 'status.json')
    try:
        d = Driver(repo, state, None, None, os.path.join(os.path.dirname(status), 'progress.md'),
                   status, dry_run=False, frida=True, frida_cache=False, queue='wave1',
                   base_tests=False)
    except (StatusError, ValueError) as e:
        print('wave.py remeasure: %s' % e, file=sys.stderr)
        return 1
    if args.dry:
        sys.stdout.write(d.remeasure_plan(picked, skipped, args.chunk, mode))
        return 0
    rc, out = d.remeasure(picked, skipped, args.chunk, args.branch, mode, stamp)
    sys.stdout.write(out)
    return rc


def main(argv=None, repo=None):
    _tool('wave')._line_buffer()
    repo = repo or _tool('wave').ROOT
    args, err = _parse(sys.argv[1:] if argv is None else argv)
    if err is None:
        if args.clear is not None:
            return _clear(args.clear, repo, args.queue, args.rerun,
                          args.rerun_frida)
        d, err = _build(args, repo)
    if err:
        if err[1]:
            print(err[1], file=sys.stderr)
        return err[0]
    return d.run()
