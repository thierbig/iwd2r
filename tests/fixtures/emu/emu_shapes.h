/* Fixture for tests/test_emu.py -- models the src/*.h offset-comment style
   (base-relative comments in TBar, absolute comments in the derived TFoo,
   VC6 pack(2) spacing).  Parsed by tools/emu/layout.py; never compiled. */

class TBar {
public:
    /* 0000 */ BOOL m_flags;
    /* 0004 */ short m_barShort;
    virtual ~TBar();
};

class TFoo : public TBar {
public:
    /* 0008 */ char m_tag;
    /* 000A */ int m_count;                 // pack(2): int at an even offset
    /* 000E */ char m_bytes[4];
    /* 0012 */ TBar* m_bar;
    /* 0016 */ RESID m_exeOnly;             // exe-only member, unknown type
    /* 001A */ TFooKind m_kind;             // enum-typed, no 'enum ' prefix
    static int s_classData;                 // static: never a data member
    int helper();                           // method: no offset comment
    /* 001E */ virtual void* hook(int a);   // commented method: skipped
};
