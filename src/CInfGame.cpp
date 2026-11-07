#include "CInfGame.h"

#include "CAIGroup.h"
#include "CAIObjectType.h"
#include "CAIScript.h"
#include "CBaldurChitin.h"
#include "CBaldurEngine.h"
#include "CBaldurProjector.h"
#include "CButtonData.h"
#include "CCacheStatus.h"
#include "CCreatureFile.h"
#include "CGameAnimationType.h"
#include "CGameArea.h"
#include "CGameContainer.h"
#include "CGameEffect.h"
#include "CGameObject.h"
#include "CGameSprite.h"
#include "CGameTrigger.h"
#include "CGameSpriteEquipment.h"
#include "CInfCursor.h"
#include "CItem.h"
#include "CMessage.h"
#include "CPathSearch.h"
#include "CResCHR.h"
#include "CResCache.h"
#include "CResText.h"
#include "CScreenChapter.h"
#include "CScreenCharacter.h"
#include "CScreenCreateChar.h"
#include "CScreenInventory.h"
#include "CScreenJournal.h"
#include "CScreenLoad.h"
#include "CScreenMap.h"
#include "CScreenOptions.h"
#include "CScreenSave.h"
#include "CScreenSinglePlayer.h"
#include "CScreenSpellbook.h"
#include "CScreenStore.h"
#include "CScreenWorld.h"
#include "CScreenWorldMap.h"
#include "CSearchBitmap.h"
#include "CSpell.h"
#include "CStore.h"
#include "CUIControlBase.h"
#include "CUIManager.h"
#include "CUIPanel.h"
#include "CUtil.h"
#include "CVariableHash.h"
#include "Icewind586B70.h"
#include "IcewindMisc.h"
#include "compat/marshal_ptr.h"
#include "compat/path_hook.h"

#define FIFTY_THREE 53

// 0x851884
const SHORT CInfGame::KILL_INNOCENT = 0;

// 0x851886
const SHORT CInfGame::INJURE_INNOCENT = 1;

// 0x851888
const SHORT CInfGame::STEAL = 2;

// 0x85188A
const SHORT CInfGame::KILL_FIST = 3;

// 0x85188C
const SHORT CInfGame::MONEY_CHURCH = 4;

// 0x85188E
const SHORT CInfGame::MONEY_BEGGAR = 5;

// 0x851890
const SHORT CInfGame::MAGIC_CHURCH = 6;

// 0x851892
const SHORT CInfGame::KILL_EVIL = 7;

// 0x851894
const SHORT CInfGame::BASE_CHURCH = 8;

// 0x851896
const SHORT CInfGame::REPUTATION_MULTIPLIER = 10;

// 0x8518AC
const DWORD CInfGame::PROGRESSBAR_AREA_ADDITIONAL = 3000000;

// 0x8518B0
const DWORD CInfGame::PROGRESSBAR_GAME_ADDITIONAL = 5000000;

// 0x8518B4
const DWORD CInfGame::PROGRESSBAR_REQUESTS_ADDITIONAL = 5000000;

// 0x8518B8
const DWORD CInfGame::PROGRESSBAR_CACHING_ADDITIONAL = 3000000;

// 0x8518BE
const SHORT CInfGame::FEEDBACK_GOLD = 1;

// 0x8518C0
const SHORT CInfGame::FEEDBACK_EXPERIENCE = 2;

// 0x8518C2
const SHORT CInfGame::FEEDBACK_ITEMGAINED = 3;

// 0x8518C4
const SHORT CInfGame::FEEDBACK_ITEMLOST = 4;

// 0x8518C6
const SHORT CInfGame::FEEDBACK_REPUTATION = 5;

// 0x8518C8
const SHORT CInfGame::FEEDBACK_TIMEPASSEDJOURNEY = 6;

// 0x8518CA
const SHORT CInfGame::FEEDBACK_TIMEPASSEDREST = 7;

// 0x8518CC
const SHORT CInfGame::FEEDBACK_INVENTORYFULL_ITEMDROPPED = 8;

// 0x8518CE
const SHORT CInfGame::FEEDBACK_JOURNAL_UPDATE = 9;

// 0x8518D0
const SHORT CInfGame::FEEDBACK_WORLDMAP_UPDATE = 10;

// 0x8AFC50
static const char* off_8AFC50[FIFTY_THREE] = {
    "Inventory",
    "Character Record",
    "Return to Game",
    "Journal",
    "Map",
    "Spellbook",
    "Options",
    "Character Arbitration",
    "Guard / Protect",
    "Turn Undead",
    "Battle Song",
    "Cast Spell",
    "Use Item",
    "Dialog",
    "Thieving",
    "Stealth",
    "Trap Detection",
    "Group Stop",
    "Group Attack",
    "Special Abilities",
    "Equip Melee Weapon",
    "Equip Range Weapon",
    "Quick Load",
    "Hide Interface",
    "Chat Window",
    "Location",
    "Quick Save",
    "Toggle AI",
    "Toggle Interface",
    "Rest",
    "Toggle Portrait HP",
    "Formation",
    "Multiselect 1-2",
    "Multiselect 3-4",
    "Multiselect 5-6",
    "Multiselect 1-3",
    "Multiselect 4-6",
    "Multiselect All",
    "Quick Spell Slot1",
    "Quick Spell Slot2",
    "Quick Spell Slot3",
    "Quick Weapon Slot1",
    "Quick Weapon Slot2",
    "Quick Weapon Slot3",
    "Quick Weapon Slot4",
    "Quick Item Slot1",
    "Quick Item Slot2",
    "Quick Item Slot3",
    "Quick Formation Slot1",
    "Quick Formation Slot2",
    "Quick Formation Slot3",
    "Quick Formation Slot4",
    "Quick Formation Slot5",
};

// 0x8AFD24
static const char* off_8AFD24[FIFTY_THREE] = {
    "I",
    "R",
    "G",
    "J",
    "M",
    "S",
    "O",
    "C",
    "",
    "",
    "",
    "",
    "",
    "D",
    "",
    "",
    "",
    "",
    "",
    "V",
    "",
    "",
    "L",
    "H",
    "T",
    "X",
    "Q",
    "A",
    "Y",
    "Z",
    ".",
    "F",
    "7",
    "8",
    "9",
    "0",
    "-",
    "=",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
};

// 0x8B4F84
const char* off_8B4F84[CINFGAME_KEYMAP_SIZE - FIFTY_THREE] = {
    "SPPR101",
    "SPPR102",
    "SPPR103",
    "SPPR105",
    "SPPR106",
    "SPPR107",
    "SPPR108",
    "SPPR109",
    "SPPR110",
    "SPPR111",
    "SPPR112",
    "SPPR113",
    "SPPR114",
    "SPPR115",
    "SPPR116",
    "SPPR117",
    "SPPR201",
    "SPPR202",
    "SPPR203",
    "SPPR204",
    "SPPR205",
    "SPPR206",
    "SPPR207",
    "SPPR208",
    "SPPR210",
    "SPPR211",
    "SPPR212",
    "SPPR213",
    "SPPR214",
    "SPPR215",
    "SPPR216",
    "SPPR217",
    "SPPR218",
    "SPPR219",
    "SPPR220",
    "SPPR221",
    "SPPR222",
    "SPPR301",
    "SPPR302",
    "SPPR303",
    "SPPR304",
    "SPPR305",
    "SPPR306",
    "SPPR307",
    "SPPR308",
    "SPPR309",
    "SPPR310",
    "SPPR311",
    "SPPR312",
    "SPPR313",
    "SPPR314",
    "SPPR315",
    "SPPR316",
    "SPPR318",
    "SPPR319",
    "SPPR320",
    "SPPR322",
    "SPPR323",
    "SPPR324",
    "SPPR325",
    "SPPR326",
    "SPPR327",
    "SPPR328",
    "SPPR401",
    "SPPR402",
    "SPPR403",
    "SPPR404",
    "SPPR405",
    "SPPR406",
    "SPPR407",
    "SPPR408",
    "SPPR409",
    "SPPR410",
    "SPPR411",
    "SPPR414",
    "SPPR415",
    "SPPR416",
    "SPPR417",
    "SPPR418",
    "SPPR420",
    "SPPR422",
    "SPPR423",
    "SPPR424",
    "SPPR425",
    "SPPR426",
    "SPPR427",
    "SPPR428",
    "SPPR429",
    "SPPR501",
    "SPPR502",
    "SPPR503",
    "SPPR504",
    "SPPR507",
    "SPPR508",
    "SPPR510",
    "SPPR511",
    "SPPR512",
    "SPPR513",
    "SPPR514",
    "SPPR515",
    "SPPR516",
    "SPPR517",
    "SPPR518",
    "SPPR519",
    "SPPR520",
    "SPPR521",
    "SPPR522",
    "SPPR523",
    "SPPR524",
    "SPPR602",
    "SPPR605",
    "SPPR606",
    "SPPR607",
    "SPPR608",
    "SPPR609",
    "SPPR610",
    "SPPR611",
    "SPPR612",
    "SPPR613",
    "SPPR614",
    "SPPR615",
    "SPPR616",
    "SPPR702",
    "SPPR704",
    "SPPR705",
    "SPPR707",
    "SPPR709",
    "SPPR712",
    "SPPR714",
    "SPPR715",
    "SPPR716",
    "SPPR717",
    "SPPR718",
    "SPPR719",
    "SPPR720",
    "SPPR721",
    "SPPR722",
    "SPPR723",
    "SPPR724",
    "SPPR725",
    "SPPR726",
    "SPPR727",
    "SPPR728",
    "SPPR729",
    "SPWI101",
    "SPWI102",
    "SPWI103",
    "SPWI104",
    "SPWI105",
    "SPWI107",
    "SPWI108",
    "SPWI112",
    "SPWI113",
    "SPWI114",
    "SPWI115",
    "SPWI116",
    "SPWI117",
    "SPWI118",
    "SPWI119",
    "SPWI120",
    "SPWI121",
    "SPWI122",
    "SPWI201",
    "SPWI203",
    "SPWI205",
    "SPWI206",
    "SPWI207",
    "SPWI209",
    "SPWI210",
    "SPWI211",
    "SPWI212",
    "SPWI213",
    "SPWI214",
    "SPWI215",
    "SPWI217",
    "SPWI218",
    "SPWI219",
    "SPWI220",
    "SPWI221",
    "SPWI222",
    "SPWI223",
    "SPWI224",
    "SPWI225",
    "SPWI226",
    "SPWI227",
    "SPWI228",
    "SPWI302",
    "SPWI303",
    "SPWI304",
    "SPWI305",
    "SPWI306",
    "SPWI308",
    "SPWI309",
    "SPWI310",
    "SPWI311",
    "SPWI312",
    "SPWI313",
    "SPWI314",
    "SPWI316",
    "SPWI317",
    "SPWI318",
    "SPWI319",
    "SPWI320",
    "SPWI321",
    "SPWI322",
    "SPWI323",
    "SPWI401",
    "SPWI402",
    "SPWI404",
    "SPWI405",
    "SPWI406",
    "SPWI407",
    "SPWI408",
    "SPWI410",
    "SPWI411",
    "SPWI412",
    "SPWI413",
    "SPWI414",
    "SPWI417",
    "SPWI418",
    "SPWI419",
    "SPWI420",
    "SPWI421",
    "SPWI422",
    "SPWI423",
    "SPWI424",
    "SPWI425",
    "SPWI426",
    "SPWI427",
    "SPWI428",
    "SPWI501",
    "SPWI503",
    "SPWI504",
    "SPWI505",
    "SPWI507",
    "SPWI508",
    "SPWI509",
    "SPWI510",
    "SPWI511",
    "SPWI512",
    "SPWI513",
    "SPWI514",
    "SPWI515",
    "SPWI516",
    "SPWI517",
    "SPWI518",
    "SPWI519",
    "SPWI520",
    "SPWI521",
    "SPWI522",
    "SPWI523",
    "SPWI524",
    "SPWI525",
    "SPWI526",
    "SPWI601",
    "SPWI603",
    "SPWI605",
    "SPWI606",
    "SPWI607",
    "SPWI608",
    "SPWI609",
    "SPWI610",
    "SPWI611",
    "SPWI612",
    "SPWI613",
    "SPWI614",
    "SPWI615",
    "SPWI616",
    "SPWI617",
    "SPWI618",
    "SPWI619",
    "SPWI620",
    "SPWI621",
    "SPWI622",
    "SPWI623",
    "SPWI624",
    "SPWI626",
    "SPWI627",
    "SPWI628",
    "SPWI629",
    "SPWI630",
    "SPWI702",
    "SPWI703",
    "SPWI704",
    "SPWI705",
    "SPWI706",
    "SPWI707",
    "SPWI708",
    "SPWI709",
    "SPWI710",
    "SPWI711",
    "SPWI712",
    "SPWI713",
    "SPWI714",
    "SPWI715",
    "SPWI716",
    "SPWI717",
    "SPWI718",
    "SPWI719",
    "SPWI802",
    "SPWI803",
    "SPWI804",
    "SPWI805",
    "SPWI806",
    "SPWI807",
    "SPWI808",
    "SPWI809",
    "SPWI810",
    "SPWI902",
    "SPWI903",
    "SPWI905",
    "SPWI906",
    "SPWI907",
    "SPWI908",
    "SPWI909",
    "SPWI910",
    "SPWI911",
    NULL,
    NULL,
};

// 0x8ABA24
static char byte_8ABA24[CINFGAME_SPECIAL_SYMBOLS] = {
    ';',
    '=',
    ',',
    '-',
    '.',
    '/',
    '`',
    '[',
    '\\',
    ']',
    '\'',
    '\0',
};

// 0x8ABA3C
static BYTE byte_8ABA3C[CINFGAME_SPECIAL_SYMBOLS] = {
    VK_OEM_1,
    VK_OEM_PLUS,
    VK_OEM_COMMA,
    VK_OEM_MINUS,
    VK_OEM_PERIOD,
    VK_OEM_2,
    VK_OEM_3,
    VK_OEM_4,
    VK_OEM_5,
    VK_OEM_6,
    VK_OEM_7,
    0,
};

// 0x8E707C
const CString CInfGame::FOGOWAR_RESREF("FOGOWAR");

// 0x8E74AC
const CString CInfGame::SAVE_OBJECT_LIST_NAME("SVTRIOBJ");

// 0x8E74F4
const CString CInfGame::CHAPTER_GLOBAL("CHAPTER");

// 0x8E74E8
const CString CInfGame::TOKEN_CURRENT_CHAPTER("CurrentChapter");

// 0x8E7510
const CString CInfGame::TOKEN_CHARACTER("CHARNAME");

// 0x8E7498
const CString CInfGame::TOKEN_GABBER("GABBER");

// 0x8E7064
const CString CInfGame::TOKEN_SIRMAAM("PROTAGONIST_SIRMAAM");

// 0x8E7084
const CString CInfGame::TOKEN_GIRLBOY("PROTAGONIST_GIRLBOY");

// 0x8E74FC
const CString CInfGame::TOKEN_BROTHERSISTER("PROTAGONIST_BROTHERSISTER");

// 0x8E7060
const CString CInfGame::TOKEN_LADYLORD("PROTAGONIST_LADYLORD");

// 0x8E7520
const CString CInfGame::TOKEN_MALEFEMALE("PROTAGONIST_MALEFEMALE");

// 0x8E7518
const CString CInfGame::TOKEN_HESHE("PROTAGONIST_HESHE");

// 0x8E7504
const CString CInfGame::TOKEN_HISHER("PROTAGONIST_HISHER");

// 0x8E74DC
const CString CInfGame::TOKEN_RACE("PROTAGONIST_RACE");

// 0x8E74D0
const CString CInfGame::TOKEN_HIMHER("PROTAGONIST_HIMHER");

// 0x8E7074
const CString CInfGame::TOKEN_MANWOMAN("PROTAGONIST_MANWOMAN");

// 0x8E74C4
const CString CInfGame::TOKEN_GABBER_SIRMAAM("SIRMAAM");

// 0x8E7070
const CString CInfGame::TOKEN_GABBER_GIRLBOY("GIRLBOY");

// 0x8E74E4
const CString CInfGame::TOKEN_GABBER_BROTHERSISTER("BROTHERSISTER");

// 0x8E7088
const CString CInfGame::TOKEN_GABBER_LADYLORD("LADYLORD");

// 0x8E7080
const CString CInfGame::TOKEN_GABBER_MALEFEMALE("MALEFEMALE");

// 0x8E751C
const CString CInfGame::TOKEN_GABBER_HESHE("HESHE");

// 0x8E74EC
const CString CInfGame::TOKEN_GABBER_HISHER("HISHER");

// 0x8E708C
const CString CInfGame::TOKEN_GABBER_RACE("RACE");

// 0x8E74C8
const CString CInfGame::TOKEN_GABBER_HIMHER("HIMHER");

// 0x8E74F0
const CString CInfGame::TOKEN_GABBER_MANWOMAN("MANWOMAN");

// 0x8E7494
const CString CInfGame::TOKEN_PLAYER1("PLAYER1");

// 0x8E74BC
const CString CInfGame::TOKEN_PLAYER2("PLAYER2");

// 0x8E7490
const CString CInfGame::TOKEN_PLAYER3("PLAYER3");

// 0x8E74A4
const CString CInfGame::TOKEN_PLAYER4("PLAYER4");

// 0x8E74C0
const CString CInfGame::TOKEN_PLAYER5("PLAYER5");

// 0x8E749C
const CString CInfGame::TOKEN_PLAYER6("PLAYER6");

// 0x8E74D8
const CString CInfGame::TOKEN_SURFACE_UNDERDARK("SURFACE_UNDERDARK");

// 0x8E74D4
const CString CInfGame::AUTO_SAVE_NAME("000000000-Auto-Save");

// 0x8E7508
const CString CInfGame::QUICK_SAVE_NAME("000000002-Quick-Save");

// 0x8E750C
const CString CInfGame::QUICK_SAVE_BACKUP_NAME("000000001-Quick-Save-Backup");

// 0x8E706C
const CString CInfGame::SILHOUETTE_PORTRAIT_SM("noportsm");

// 0x8E705C
const CString CInfGame::SILHOUETTE_PORTRAIT_LG("noportlg");

// 0x8E7524
BOOL CInfGame::m_bHealPartyOnRest;

// 0x8E7528
BOOLEAN CInfGame::byte_8E7528;

// 0x8E752C
int CInfGame::dword_8E752C;

// 0x8E52EC
int CInfGame::dword_8E52EC;

// 0x8E52F0
int CInfGame::dword_8E52F0;

// 0x59CC30
CInfGame::CInfGame()
    : m_rgbMasterBitmap(CResRef("MPALETTE"), FALSE, 24)
{
    m_nUniqueAreaID = 0;
    m_nReputation = 0;
    m_nProtagonistId = 0;
    m_bPartyAI = FALSE;
    m_bShowAreaNotes = FALSE;
    m_dwLastProgressRenderTickCount = 0;
    m_dwLastProgressMsgTickCount = 0;
    m_nLastSaveTime = 0;
    m_lastTarget = 0;
    field_4AB2 = 0;
    m_nCharacterTerminationSequenceDelay = 0;
    m_nAIIndex = 0;
    m_nTimeStop = 0;
    m_nTimeStopCaster = 0;
    memset(m_appearanceColorsWeapon, 0, sizeof(m_appearanceColorsWeapon));
    memset(m_appearanceColorsBody, 0, sizeof(m_appearanceColorsBody));
    memset(m_appearanceColorsShield, 0, sizeof(m_appearanceColorsShield));
    memset(m_appearanceColorsHelmet, 0, sizeof(m_appearanceColorsHelmet));
    m_nDifficultyLevel = 0;
    m_nCharacters = 0;
    for (INT nIndex = 0; nIndex < CINFGAME_MAXCHARACTERS; nIndex++) {
        m_characters[nIndex] = CGameObjectArray::INVALID_INDEX;
        m_characterPortraits[nIndex] = CGameObjectArray::INVALID_INDEX;
    }

    m_nState = 0;
    m_tempCursor = 4;
    m_iconIndex = -1;
    m_bGameLoaded = FALSE;
    m_bInLoadGame = FALSE;
    m_bInLoadArea = FALSE;
    m_bFromNewGame = FALSE;
    m_bInDestroyGame = FALSE;
    m_bInAreaTransition = FALSE;
    m_visibleArea = 0;
    memset(m_gameAreas, 0, sizeof(m_gameAreas));
    m_pGameAreaMaster = NULL;
    m_pathSearch = NULL;
    m_listGrid = NULL;
    m_searchRequestListEmpty = TRUE;
    m_iContainer = CGameObjectArray::INVALID_INDEX;
    m_iContainerSprite = CGameObjectArray::INVALID_INDEX;
    m_bForceDither = FALSE;
    m_sSaveDir = ".\\save\\";
    m_sMultiplayerSaveDir = ".\\mpsave\\";
    m_sTempDir = ".\\temp\\";
    m_sTempSaveDir = ".\\tempsave\\";
    m_sScriptsDir = ".\\scripts\\";
    m_sPortraitsDir = ".\\portraits\\";
    m_sSoundsDir = ".\\sounds\\";
    m_sCharactersDir = ".\\characters\\";

    if (!g_pChitin->cDimm.DirectoryRemoveFiles(m_sTempDir)) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 2608
        UTIL_ASSERT_MSG(FALSE, "Problems removing files from Temp directory, delete the directory and try again");
    }

    if (!g_pChitin->cDimm.DirectoryRemoveFiles(m_sTempSaveDir)) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 2608
        UTIL_ASSERT_MSG(FALSE, "Problems removing files from TempSave directory, delete the directory and try again");
    }

    g_pChitin->cDimm.AddToDirectoryList(m_sScriptsDir, TRUE);
    g_pChitin->cDimm.AddToDirectoryList(m_sSoundsDir, TRUE);
    g_pChitin->cDimm.AddToDirectoryList(m_sPortraitsDir, TRUE);
    g_pChitin->cDimm.AddToDirectoryList(m_sCharactersDir, TRUE);

    m_gameSave.m_curFormation = TRUE;
    m_gameSave.field_1AC = FALSE;

    field_4A42 = 48;
    field_4A44 = 48;
    m_gameSave.m_mode = -1;
    m_gameSave.m_cutScene = FALSE;
    m_currArmor = '1';
    field_4A46 = -1;
    m_currAnimation = -1;
    m_vcLocator.SetResRef(CResRef("locater"), FALSE, TRUE, TRUE);
    memset(m_defaultFamiliarResRefs, 0, sizeof(m_defaultFamiliarResRefs));
    field_4204 = 0;
    m_nCharacterOverflowCount = 0;
    m_saveObjectList.LoadList(CResRef(SAVE_OBJECT_LIST_NAME), FALSE);
    m_listGrid = new CPathNode*[CPathSearch::GRID_ACTUALX * CPathSearch::GRID_ACTUALY];
    m_pathSearch = new CPathSearch(m_listGrid);
    m_searchShutdown = FALSE;
    m_hSearchThread = CreateSemaphoreA(NULL, 0, 1, NULL);
    if (m_hSearchThread != NULL) {
        m_nTravelScreenImageToUse = 0;
        m_bPlayerScriptStyle = FALSE;
        field_4B38 = 0;
        field_366E = 0;
        m_nAreaFirstObject = 0;

        m_INISounds.SetFileName(CString("sounds"));
        if (m_INISounds.Load(CString("sounds")) == 0) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 2671
            UTIL_ASSERT_MSG(FALSE, "Problem loading the sounds.ini file!");
        }

        memset(m_aServerStore, 0, sizeof(m_aServerStore));
        memset(m_nServerStoreDemands, 0, sizeof(m_nServerStoreDemands));

        field_50D8 = FALSE;
        field_50DC = 0;

        CString sFileName;
        g_pChitin->cDimm.WriteSetUp(m_sTempDir + "foo.bar", sFileName);
        g_pChitin->cDimm.WriteSetUp(m_sTempSaveDir + "foo.bar", sFileName);

        field_4BD4 = 0;
        m_bExpansion = FALSE;
        field_4BD6 = FALSE;

        C2DArray tSpells;
        tSpells.Load(CResRef(CRuleTables::LISTSPLL));
        m_spells.Load(tSpells, 7);

        for (unsigned int spellcasterClassIndex = 0; spellcasterClassIndex < 7; spellcasterClassIndex++) {
            m_spellsByClass[spellcasterClassIndex].Load(tSpells,
                spellcasterClassIndex,
                7,
                NULL);
        }

        C2DArray tDomainSpells;
        tDomainSpells.Load(CResRef(CRuleTables::LISTDOMN));

        for (unsigned int domainIndex = 0; domainIndex < 9; domainIndex++) {
            m_spellsByDomain[domainIndex].Load(tDomainSpells,
                domainIndex,
                9,
                &m_spells);
        }

        C2DArray tInnateSpells;
        tInnateSpells.Load(CResRef(CRuleTables::LISTINNT));
        m_innateSpells.Load(tInnateSpells, 0);

        C2DArray tSongs;
        tSongs.Load(CResRef(CRuleTables::LISTSONG));
        m_songs.Load(tSongs, 0);

        C2DArray tShapeshifts;
        tShapeshifts.Load(CResRef(CRuleTables::LISTSHAP));
        m_shapeshifts.Load(tShapeshifts, 0);

        // NOTE: The following validation code looks incomplete. Logging stuff
        // was likely compiled out via debug/release switches.
        CString sError(" ");
        UINT nIndex;

        for (nIndex = 0; nIndex < m_spells.m_nCount; nIndex++) {
            CSpell cSpell(m_spells.Get(nIndex));

            sError.Format("Unable to load Class Spell file: %s\n",
                (LPCSTR)m_spells.Get(nIndex).GetResRefStr());

            if (cSpell.Demand()) {
                sError.Format("Invalid Class Spell level: %s - %d\n",
                    (LPCSTR)m_spells.Get(nIndex).GetResRefStr(),
                    cSpell.GetLevel());
            }

            cSpell.Release();
        }

        for (nIndex = 0; nIndex < m_innateSpells.m_nCount; nIndex++) {
            CSpell cSpell(m_innateSpells.Get(nIndex));

            sError.Format("Unable to load Innate Spell file: %s\n",
                (LPCSTR)m_innateSpells.Get(nIndex).GetResRefStr());

            if (cSpell.Demand()) {
                sError.Format("Invalid Innate Spell level: %s - %d\n",
                    (LPCSTR)m_innateSpells.Get(nIndex).GetResRefStr(),
                    cSpell.GetLevel());
            }

            cSpell.Release();
        }

        for (nIndex = 0; nIndex < m_songs.m_nCount; nIndex++) {
            CSpell cSpell(m_songs.Get(nIndex));

            sError.Format("Unable to load Song file: %s\n",
                (LPCSTR)m_songs.Get(nIndex).GetResRefStr());

            if (cSpell.Demand()) {
                sError.Format("Invalid Song level: %s - %d\n",
                    (LPCSTR)m_songs.Get(nIndex).GetResRefStr(),
                    cSpell.GetLevel());
            }

            cSpell.Release();
        }

        for (nIndex = 0; nIndex < m_shapeshifts.m_nCount; nIndex++) {
            CSpell cSpell(m_shapeshifts.Get(nIndex));

            sError.Format("Unable to load Shapeshift file: %s\n",
                (LPCSTR)m_shapeshifts.Get(nIndex).GetResRefStr());

            if (cSpell.Demand()) {
                sError.Format("Invalid Shapeshift level: %s - %d\n",
                    (LPCSTR)m_shapeshifts.Get(nIndex).GetResRefStr(),
                    cSpell.GetLevel());
            }

            cSpell.Release();
        }
    }
}

// 0x59ECB0
CInfGame::~CInfGame()
{
    CSingleLock searchLock(&field_1B58, FALSE);

    if (m_bGameLoaded) {
        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
            g_pChitin->cNetwork.CloseSession(TRUE);
        }
        DestroyGame(FALSE, FALSE);
    }

    if (m_hSearchThread != NULL) {
        searchLock.Lock(INFINITE);
        m_searchShutdown = TRUE;
        ReleaseSemaphore(m_hSearchThread, 1, NULL);
        searchLock.Unlock();

        while (m_hSearchThread != NULL) {
            SleepEx(1, FALSE);
        }

        if (m_pathSearch != NULL) {
            delete m_pathSearch;
            m_pathSearch = NULL;
        }

        if (m_listGrid != NULL) {
            delete[] m_listGrid;
            m_listGrid = NULL;
        }

        m_allies.RemoveAll();
        m_familiars.RemoveAll();

        g_pChitin->cDimm.RemoveFromDirectoryList(m_sScriptsDir, FALSE);
        g_pChitin->cDimm.RemoveFromDirectoryList(m_sSoundsDir, FALSE);
        g_pChitin->cDimm.RemoveFromDirectoryList(m_sPortraitsDir, FALSE);
        g_pChitin->cDimm.RemoveFromDirectoryList(m_sCharactersDir, FALSE);

        Icewind586B70::Destroy();
    }
}

// 0x59F500
void CInfGame::StartSearchThread()
{
    if (_beginthread(SearchThreadMain, 0, NULL) == -1) {
        CloseHandle(m_hSearchThread);
        m_hSearchThread = NULL;
    }
}

// 0x59F540
void CInfGame::InitGame(BOOLEAN bProgressBarRequired, BOOLEAN bProgressBarInPlace)
{
    m_cVRamPool.AttachSurfaces(g_pBaldurChitin->GetCurrentVideoMode());

    if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL) {
        m_singlePlayerPermissions.SetSinglePermission(CGamePermission::AREA_TRANSITION, TRUE);
        m_singlePlayerPermissions.SetSinglePermission(CGamePermission::CHAR_RECORDS, TRUE);
        m_singlePlayerPermissions.SetSinglePermission(CGamePermission::DIALOG, TRUE);
        m_singlePlayerPermissions.SetSinglePermission(CGamePermission::GROUP_POOL, TRUE);
        m_singlePlayerPermissions.SetSinglePermission(CGamePermission::LEADER, TRUE);
        m_singlePlayerPermissions.SetSinglePermission(CGamePermission::MODIFY_CHARS, TRUE);
        m_singlePlayerPermissions.SetSinglePermission(CGamePermission::PAUSING, TRUE);
        m_singlePlayerPermissions.SetSinglePermission(CGamePermission::PURCHASING, TRUE);
    }

    if (g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
        m_singlePlayerPermissions.SetSinglePermission(CGamePermission::LEADER, TRUE);
    }

    LoadKeymap();
    LoadOptions();

    g_pBaldurChitin->EnginesGameInit();

    m_cButtonArray.SetState(CInfButtonArray::STATE_NONE, 0);

    g_pBaldurChitin->GetCurrentVideoMode()->rgbGlobalTint = RGB(255, 255, 255);

    m_worldTime.m_gameTime = CTimerWorld::TIME_DAY + 1;
    m_worldTime.CheckForTriggerEventPast();

    g_pBaldurChitin->GetTlkTable().Fetch(16484, field_4814);
    g_pBaldurChitin->GetTlkTable().Fetch(15307, field_487C);

    for (BYTE nIndex = 0; nIndex < 6; nIndex++) {
        EnablePortrait(nIndex, FALSE);
    }

    m_gameSave.field_1B0 = 0;
    memset(m_gameSave.m_groupInventory, 0, sizeof(m_gameSave.m_groupInventory));
    m_gameSave.m_mode = -1;
    m_gameSave.m_cutScene = FALSE;
    m_appearanceWeapon = "";
    m_appearanceBody = "";
    m_appearanceShield = "";
    m_appearanceHelmet = "";

    m_pGameAreaMaster = NULL;
    m_bPartyAI = TRUE;
    m_bShowAreaNotes = TRUE;
    field_4AB2 = 0;
    m_bGameLoaded = TRUE;

    m_allies.RemoveAll();
    m_familiars.RemoveAll();

    memset(m_defaultFamiliarResRefs, 0, sizeof(m_defaultFamiliarResRefs));

    field_4204 = 0;

    m_cJournal.ClearAllEntries();

    m_variables.Resize(2048);
    m_namedCreatures.Resize(2048);
    m_variables.ClearAll();
    m_namedCreatures.ClearAll();

    m_entanglePalette.SetType(CVidPalette::TYPE_RANGE);
    m_entanglePalette.SetRange(0, 54, m_rgbMasterBitmap);

    m_webHoldPalette.SetType(CVidPalette::TYPE_RANGE);
    m_webHoldPalette.SetRange(0, 65, m_rgbMasterBitmap);

    sub_5A0160();

    m_nCharacterTerminationSequenceDelay = 0;

    m_cMoveList.ClearAll();
    m_cLimboList.ClearAll();

    CGameAIGame* pAIGame = new CGameAIGame();
    CAIScript* pScript = new CAIScript(CResRef("BALDUR"));

    // NOTE: Uninline.
    pAIGame->SetDefaultScript(pScript);

    m_nAIIndex = pAIGame->m_id;
    m_nTimeStop = 0;
    m_nTimeStopCaster = -1;

    if (!g_pChitin->cDimm.DirectoryRemoveFiles(m_sTempDir)) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 3213
        UTIL_ASSERT_MSG(FALSE, "Could not clean out Temp directory");
    }

    if (!g_pChitin->cDimm.DirectoryRemoveFiles(m_sTempSaveDir)) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 3215
        UTIL_ASSERT_MSG(FALSE, "Could not clean out TempSave directory");
    }

    // TODO: Use loop.
    memset(m_aServerStore, 0, sizeof(m_aServerStore));
    memset(m_nServerStoreDemands, 0, sizeof(m_nServerStoreDemands));

    dword_8E752C = 0;

    ResetMultiPlayerPermissions();
}

// 0x59F9A0
void CInfGame::BeginListManipulation(CGameArea* pArea)
{
    if (pArea != NULL) {
        EnterCriticalSection(&(pArea->field_214));
        pArea->SetListManipulationThreadId(GetCurrentThreadId());
    }
}

// 0x59F9D0
INT CInfGame::EndListManipulation(CGameArea* pArea)
{
    INT nCounter = 0;

    if (pArea != NULL) {
        nCounter = pArea->SetListManipulationThreadId(0);
        LeaveCriticalSection(&(pArea->field_214));
    }

    return nCounter;
}

// 0x59FA00
void CInfGame::sub_59FA00(BOOL a1)
{
    CGameArea* pArea = GetVisibleArea();
    if (pArea != NULL) {
        INT nCounter = pArea->m_nListManipulationThreadCounter;
        if (pArea->m_ListManipulationThreadId == GetCurrentThreadId()) {
            while (1) {
                CGameArea* pArea = GetVisibleArea();
                if (pArea == NULL) {
                    break;
                }

                INT nRemainingCounter = pArea->SetListManipulationThreadId(0);
                LeaveCriticalSection(&(pArea->field_214));

                if (nRemainingCounter == 0) {
                    break;
                }
            }

            while (g_pChitin->m_bDisplayStale == TRUE
                || (g_pChitin->m_bInSynchronousUpdate == TRUE && a1)) {
                SleepEx(5, FALSE);
            }

            while (nCounter != 0) {
                CGameArea* pArea = GetVisibleArea();
                if (pArea != NULL) {
                    EnterCriticalSection(&(pArea->field_214));
                    pArea->SetListManipulationThreadId(GetCurrentThreadId());
                }
                nCounter--;
            }
        } else {
            while (g_pChitin->m_bInSynchronousUpdate == TRUE
                || (g_pChitin->m_bDisplayStale == TRUE && a1)) {
                SleepEx(5, FALSE);
            }
        }
    } else {
        while (g_pChitin->m_bInSynchronousUpdate == TRUE
            || (g_pChitin->m_bDisplayStale == TRUE && a1)) {
            SleepEx(5, FALSE);
        }
    }
}

// 0x59FB50
void CInfGame::DestroyGame(BOOLEAN bProgressBarRequired, BOOLEAN bProgressBarInPlace)
{
    DWORD dwPerSegment;
    BYTE cnt;

    if (!bProgressBarInPlace) {
        if (bProgressBarRequired == TRUE) {
            sub_59FA00(TRUE);
            g_pChitin->SetProgressBar(TRUE,
                11831,
                0,
                0,
                FALSE,
                0,
                FALSE,
                0,
                FALSE,
                FALSE,
                255);
            g_pChitin->cProgressBar.m_nActionProgress = 0;
            g_pChitin->cProgressBar.m_nActionTarget = 5000000;
            g_pChitin->cProgressBar.m_bDisableMinibars = TRUE;
        }
    }

    dwPerSegment = 555555;

    CSingleLock lock(&(g_pBaldurChitin->GetObjectGame()->field_1B58), TRUE);
    m_bInDestroyGame = TRUE;

    for (cnt = 0; cnt < CINFGAME_MAX_AREAS; cnt++) {
        if (m_gameAreas[cnt] != NULL) {
            while (m_gameAreas[cnt]->m_bInPathSearch != FALSE) {
                SleepEx(25, FALSE);
            }
        }
    }

    if (bProgressBarInPlace || bProgressBarRequired) {
        ProgressBarCallback(dwPerSegment, FALSE);
    }

    if (g_pBaldurChitin->GetScreenWorld() != NULL) {
        g_pBaldurChitin->GetScreenWorld()->m_weather.CancelCurrentWeather(NULL, 0);
    }

    g_pChitin->cDimm.RemoveFromDirectoryList(m_sTempDir, TRUE);
    m_cObjectArray.Clean();

    EnterCriticalSection(&(g_pBaldurChitin->GetScreenWorld()->field_106));
    for (cnt = 0; cnt < CINFGAME_MAX_AREAS; cnt++) {
        if (m_gameAreas[cnt] != NULL) {
            while (m_gameAreas[cnt]->m_bInPathSearch != FALSE) {
                SleepEx(25, FALSE);
            }

            delete m_gameAreas[cnt];
            m_gameAreas[cnt] = NULL;
        }
    }
    m_visibleArea = 0;
    LeaveCriticalSection(&(g_pBaldurChitin->GetScreenWorld()->field_106));

    lock.Unlock();

    if (bProgressBarInPlace || bProgressBarRequired) {
        ProgressBarCallback(dwPerSegment, FALSE);
    }

    for (cnt = 0; cnt < CINFGAME_MAXCHARACTERS; cnt++) {
        EnablePortrait(cnt, FALSE);
        if (m_characterPortraits[cnt] != -1) {
            CGameObject* pObject;
            if (m_cObjectArray.Delete(m_characterPortraits[cnt], CGameObjectArray::THREAD_ASYNCH, &pObject, -1) == CGameObjectArray::SUCCESS
                && pObject != NULL) {
                delete pObject;
            }
            m_characterPortraits[cnt] = -1;
        }
    }

    memset(m_characters, -1, sizeof(m_characters));
    m_nCharacters = 0;

    if (bProgressBarInPlace || bProgressBarRequired) {
        ProgressBarCallback(dwPerSegment, FALSE);
    }

    while (!m_lstGlobalCreatures.IsEmpty()) {
        LONG iObject = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lstGlobalCreatures.RemoveHead()));
        CGameObject* pObject;
        if (m_cObjectArray.Delete(iObject, CGameObjectArray::THREAD_ASYNCH, &pObject, -1) == CGameObjectArray::SUCCESS
            && pObject != NULL) {
            delete pObject;
        }
    }

    for (cnt = 0; cnt < 9; cnt++) {
        while (!m_familiarResRefs[cnt]->IsEmpty()) {
            delete m_familiarResRefs[cnt]->RemoveHead();
        }
    }

    if (bProgressBarInPlace || bProgressBarRequired) {
        ProgressBarCallback(dwPerSegment, FALSE);
    }

    field_4A42 = 48;
    field_4A44 = 48;
    m_nState = 0;
    m_tempCursor = 4;
    m_iconIndex = -1;
    m_searchRequestListEmpty = TRUE;
    m_currArmor = '1';
    field_4A46 = -1;
    m_currAnimation = -1;
    m_bGameLoaded = FALSE;

    for (cnt = 0; cnt < 100; cnt++) {
        if (m_gameSave.m_groupInventory[cnt] != NULL) {
            delete m_gameSave.m_groupInventory[cnt];
            m_gameSave.m_groupInventory[cnt] = NULL;
        }
    }

    g_pBaldurChitin->GetTlkTable().m_override.CloseFiles();

    if (bProgressBarInPlace || bProgressBarRequired) {
        ProgressBarCallback(dwPerSegment, FALSE);
    }

    m_group.m_memberList.RemoveAll();
    m_group.m_groupChanged = TRUE;
    m_allies.RemoveAll();
    m_familiars.RemoveAll();
    g_pBaldurChitin->EnginesGameUninit();

    if (bProgressBarInPlace || bProgressBarRequired) {
        ProgressBarCallback(dwPerSegment, FALSE);
        // FIXME: Redundant.
        if (bProgressBarInPlace || bProgressBarRequired) {
            ProgressBarCallback(dwPerSegment, FALSE);
        }
    }

    if (!g_pChitin->cDimm.DirectoryRemoveFiles(m_sTempDir)) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 3592
        UTIL_ASSERT_MSG(FALSE, "Could not remove files from Temp directory");
    }

    if (!g_pChitin->cDimm.DirectoryRemoveFiles(m_sTempSaveDir)) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 3296
        UTIL_ASSERT_MSG(FALSE, "Could not remove files from TempSave directory");
    }

    if (bProgressBarInPlace || bProgressBarRequired) {
        ProgressBarCallback(dwPerSegment, FALSE);
    }

    m_bInDestroyGame = FALSE;

    if (bProgressBarInPlace || bProgressBarRequired) {
        ProgressBarCallback(dwPerSegment, FALSE);
    }

    if (!bProgressBarInPlace && bProgressBarRequired == TRUE) {
        g_pChitin->cProgressBar.m_nActionProgress = g_pChitin->cProgressBar.m_nActionTarget;
        g_pChitin->cProgressBar.m_bDisableMinibars = TRUE;
        g_pChitin->m_bDisplayStale = TRUE;
        sub_59FA00(TRUE);
        g_pChitin->SetProgressBar(FALSE,
            0,
            0,
            0,
            FALSE,
            0,
            FALSE,
            0,
            FALSE,
            FALSE,
            255);
    }

    m_cVRamPool.DetachSurfaces();

    for (cnt = 0; cnt < 12; cnt++) {
        if (m_aServerStore[cnt] != NULL) {
            delete m_aServerStore[cnt];
        }
        m_aServerStore[cnt] = NULL;
    }

    POSITION pos = field_4BDC.GetHeadPosition();
    while (pos != NULL) {
        POSITION posOld = pos;
        CInfGameAreaLevel* node = field_4BDC.GetNext(pos);
        if (node != NULL) {
            delete node;
        }
        field_4BDC.RemoveAt(posOld);
    }
}

// 0x5A0160
void CInfGame::sub_5A0160()  // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
{
    CString sSearchPath = g_pChitin->cDimm.cResCache.m_sDirName + "data\\*.*";
    CString sResolvedSearchPath;
    if (g_pChitin->lAliases.ResolveFileName(sSearchPath, sResolvedSearchPath) == FALSE) {
        sResolvedSearchPath = sSearchPath;
    }

    CFileFind fileFind;
    BOOLEAN bWorking = fileFind.FindFile(sResolvedSearchPath, 0);
    while (bWorking) {
        bWorking = fileFind.FindNextFile();

        CString sBifFileName = fileFind.GetFilePath();
        INT nSlash = sBifFileName.Find('\\');
        while (nSlash != -1) {
            sBifFileName = sBifFileName.Right(sBifFileName.GetLength() - nSlash - 1);
            nSlash = sBifFileName.Find('\\');
        }

        CString sBifName = sBifFileName.Left(sBifFileName.GetLength() - 4);
        CString sExtension = sBifFileName.Right(4);
        sExtension.MakeUpper();
        if (sExtension.Compare(".BIF") == 0) {
            CString sMinimumSize;
            CPoint ptLocation;
            if (m_ruleTables.m_tCacheValidation.Find(sBifName, ptLocation, FALSE) == TRUE) {
                if (m_ruleTables.m_tCacheValidation.GetWidth() < 2
                    || m_ruleTables.m_tCacheValidation.GetHeight() <= ptLocation.y
                    || ptLocation.y < 0) {
                    sMinimumSize = m_ruleTables.m_tCacheValidation.GetDefault();
                } else {
                    sMinimumSize = m_ruleTables.m_tCacheValidation.GetAt(CPoint(1, ptLocation.y));
                }
            } else {
                sMinimumSize = m_ruleTables.m_tCacheValidation.GetDefault();
            }

            g_pChitin->cDimm.cResCache.ValidateFile(g_pChitin->cDimm.GetResFileID(sBifName),
                atol(sMinimumSize));
        }
    }

    fileFind.Close();
}

// 0x5A04B0
LONG CInfGame::CachingRequirements(const CString& areaName)
{
    return 0;
}

// 0x5A0950
void CInfGame::CacheResFileWithResource(const CString& areaName)
{
    // TODO: Incomplete.
}

// 0x5A0F50
LONG CInfGame::ImportCharacter(const CString& sFileName, INT nIndex)
{
    CResHelper<CResCHR, 1018> cCharacterFile;
    BOOLEAN bTemporaryKey = FALSE;
    // NOTE: The original allocates resources of type 1022 as a class of its
    // own (vtable 0x8480C8, `Demand` 0x4029F0: the raw data, "parsed" when
    // present); `CBaldurChitin::AllocResObject` does not have it yet, so the
    // data is read through `CRes::Demand`.
    CResHelper<CRes, 1022> cBiographyFile;
    STR_RES strRes;

    // NOTE: Uninline.
    cCharacterFile.SetResRef(CResRef(sFileName), TRUE, TRUE);
    if (cCharacterFile.GetResRef() == "") {
        bTemporaryKey = TRUE;
        g_pChitin->cDimm.AddTemporaryKey(m_sCharactersDir, CResRef(sFileName), 1018);

        // NOTE: Uninline.
        cCharacterFile.SetResRef(CResRef(sFileName), TRUE, TRUE);
    }

    BYTE* pData = cCharacterFile.GetRes() != NULL
        ? reinterpret_cast<BYTE*>(cCharacterFile.GetRes()->Demand())
        : NULL;

    // NOTE: Inlined (0x720490): the resource's size, read between a
    // `Demand` and a `Release`.
    LONG nData = 0;
    if (cCharacterFile.GetRes() != NULL) {
        if (cCharacterFile.GetRes()->Demand() != NULL) {
            nData = cCharacterFile.GetRes()->m_nResSizeActual;
            cCharacterFile.GetRes()->Release();
        }
    }

    if (pData != NULL && memcmp(pData, "CHR V2.2", 8) == 0) {
        BYTE* pHeader = pData + 8;

        LONG nCreature = nData - 0x224;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 4172
        UTIL_ASSERT(nCreature > 0);

        nCreature -= *reinterpret_cast<DWORD*>(pHeader + 0x24);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 4175
        UTIL_ASSERT(nCreature >= 0);

        CGameSprite* pSprite = new CGameSprite(pData + *reinterpret_cast<DWORD*>(pHeader + 0x20),
            *reinterpret_cast<DWORD*>(pHeader + 0x24),
            0,
            0,
            -1,
            0,
            0,
            INT_MAX,
            CPoint(-1, -1),
            0);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 4180
        UTIL_ASSERT(pSprite != NULL);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 4181
        UTIL_ASSERT(nCreature == 0);

        pSprite->SetResRef(CResRef(sFileName));

        char szName[SCRIPTNAME_SIZE + 1];
        memset(szName, 0, sizeof(szName));
        strncpy(szName, reinterpret_cast<char*>(pHeader), SCRIPTNAME_SIZE);
        pSprite->m_sName = CString(szName);

        for (INT nWeapon = 0; nWeapon < 8; nWeapon++) {
            pSprite->InitQuickItemData(*reinterpret_cast<WORD*>(pHeader + 0x28 + 2 * nWeapon),
                *reinterpret_cast<WORD*>(pHeader + 0x38 + 2 * nWeapon),
                pHeader[0x19C + nWeapon],
                pSprite->m_quickWeapons[nWeapon]);
        }

        for (INT nSpell = 0; nSpell < 9; nSpell++) {
            pSprite->InitQuickSpellData(CResRef(pHeader + 0x48 + RESREF_SIZE * nSpell),
                1,
                pSprite->m_quickSpells[nSpell],
                pHeader[0x90 + nSpell],
                pHeader[0x15C + nSpell],
                pHeader[0x165 + nSpell]);
        }

        for (INT nItem = 0; nItem < 3; nItem++) {
            pSprite->InitQuickItemData(*reinterpret_cast<WORD*>(pHeader + 0x9A + 2 * nItem),
                *reinterpret_cast<WORD*>(pHeader + 0xA0 + 2 * nItem),
                0,
                pSprite->m_quickItems[nItem]);
        }

        for (INT nInnate = 0; nInnate < 9; nInnate++) {
            pSprite->InitQuickSpellData(CResRef(pHeader + 0xA6 + RESREF_SIZE * nInnate),
                2,
                pSprite->m_quickInnates[nInnate],
                0,
                0,
                0);
        }

        for (INT nSong = 0; nSong < 9; nSong++) {
            pSprite->InitQuickSpellData(CResRef(pHeader + 0xEE + RESREF_SIZE * nSong),
                3,
                pSprite->m_quickSongs[nSong],
                0,
                0,
                0);
        }

        for (INT nButton = 0; nButton < 9; nButton++) {
            pSprite->field_3D14[nButton] = *reinterpret_cast<int*>(pHeader + 0x136 + 4 * nButton);
        }

        pSprite->m_nLastSpellbookClassIndex = pHeader[0x15A];
        pSprite->m_nLastSpellbookSpellLevel = pHeader[0x15B];

        if (bTemporaryKey == TRUE) {
            g_pChitin->cDimm.RemoveTemporaryKey(m_sCharactersDir, CResRef(sFileName), 1018);
        }

        // NOTE: Uninline.
        cBiographyFile.SetResRef(CResRef(sFileName), TRUE, TRUE);
        if (cBiographyFile.GetResRef() != "") {
            STRREF strId = 0xF240 + static_cast<BYTE>(nIndex);

            const char* pText = cBiographyFile.GetRes() != NULL
                ? reinterpret_cast<const char*>(cBiographyFile.GetRes()->Demand())
                : NULL;
            strRes.szText = CString(pText);
            strRes.szText = strRes.szText.Left(cBiographyFile.GetRes()->nSize);

            g_pBaldurChitin->GetTlkTable().m_override.AddUserEntry(strId, strRes);
            pSprite->m_baseStats.m_biography = strId;

            cBiographyFile.GetRes()->Release();
        } else {
            pSprite->m_baseStats.m_biography = -1;
        }

        BYTE nClass;

        nClass = 3;
        pSprite->AddKnownDivineSpells(nClass);

        nClass = 4;
        pSprite->AddKnownDivineSpells(nClass);

        nClass = 7;
        pSprite->AddKnownDivineSpells(nClass);

        nClass = 8;
        pSprite->AddKnownDivineSpells(nClass);

        AddPartyGold(g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetStartingGold(pSprite));

        // NOTE: The sound set's folder name is read up to its terminator,
        // which a name of 32 characters lacks (the original reads on into
        // the header's next field).
        CString sSounds(reinterpret_cast<char*>(pHeader + 0x17C));
        INT nLength = sSounds.GetLength();
        memcpy(pSprite->field_725A, sSounds.GetBuffer(nLength), min(nLength, 32));
        if (nLength < 32) {
            pSprite->field_725A[nLength] = '\0';
        }

        CString sSoundsDir = g_pBaldurChitin->GetObjectGame()->GetDirSounds() + sSounds + '\\';
        g_pChitin->cDimm.AddToDirectoryList(sSoundsDir, TRUE);

        pSprite->m_secondarySounds = CResRef(pHeader + 0x174);

        cCharacterFile.GetRes()->dwFlags |= CRes::RES_FLAG_0x100;

        // NOTE: Uninline.
        cCharacterFile.Release();

        pSprite->m_bGlobal = TRUE;
        pSprite->sub_726810(pSprite->m_nWeaponSet);

        return pSprite->m_id;
    }

    // NOTE: Uninline.
    cCharacterFile.Release();

    if (bTemporaryKey) {
        g_pChitin->cDimm.RemoveTemporaryKey(m_sCharactersDir, CResRef(sFileName), 1018);

        // NOTE: Uninline.
        cCharacterFile.Release();
    }

    return 0;
}

// NOTE: SaveGame (0x5AC430), Marshal (0x5A3230), the two party-entry
// converters (0x5A79E0, 0x5A7BF0) and Unmarshal (0x5A7E40) sit here, before
// GetDirSounds (0x5A1A80), out of address order. Placed at their addresses,
// git's default diff aligns the new bodies against CanSaveGame,
// BackupQuickSave or GetSaveGames, and tools/check_evidence.py would count
// those unchanged functions as touched.

// 0x5AC430
BOOL CInfGame::SaveGame(BOOLEAN bProgressBarRequired, BOOLEAN bProgressBarInPlace, BOOLEAN bSaveScreen)
{
    CGameFile cGameFile;
    BOOL bResult;
    DWORD dwSectorsPerCluster;
    DWORD dwBytesPerSector;
    DWORD dwNumberOfFreeClusters;
    DWORD dwTotalNumberOfClusters;
    ULARGE_INTEGER nFreeBytesAvailable;
    ULARGE_INTEGER nTotalNumberOfBytes;
    HMODULE hModule;
    BYTE* pData;
    DWORD nData;
    INT cnt;

    // NOTE: Not in the original: a save is one marshal pass for the handles
    // that stand in for pointers in 32-bit file fields (compat/marshal_ptr.h).
    iwd2r::ResetMarshalPtrs();

    bResult = TRUE;

    for (cnt = 0; cnt < CINFGAME_MAX_AREAS; cnt++) {
        if (m_gameAreas[cnt] != NULL) {
            m_gameAreas[cnt]->sub_47A190();
        }
    }

    if (strcmp(m_sSaveGame, "") == 0) {
        return FALSE;
    }

    if (GetDiskFreeSpaceA(NULL, &dwSectorsPerCluster, &dwBytesPerSector, &dwNumberOfFreeClusters, &dwTotalNumberOfClusters) != TRUE
        || dwSectorsPerCluster * dwNumberOfFreeClusters < CBaldurChitin::MINIMUM_DRIVE_SECTORS
        || dwSectorsPerCluster * dwNumberOfFreeClusters < CBaldurChitin::MINIMUM_DRIVE_SPACE / dwBytesPerSector) {
        hModule = LoadLibraryA("kernel32.dll");
        if (hModule == NULL) {
            if (bProgressBarInPlace) {
                g_pChitin->SetProgressBar(FALSE, 0, 0, 0, FALSE, 0, FALSE, 0, FALSE, FALSE, 255);
            }

            g_pBaldurChitin->m_dwCloseConfirmationStrId = 11790;
            g_pBaldurChitin->m_dwCloseConfirmationFlags = 0x34;
            PostMessageA(g_pBaldurChitin->cWnd.GetSafeHwnd(), WM_CLOSE, 0, 0);
            return FALSE;
        }

        BOOL bHaveGetDiskFreeSpaceExA = GetProcAddress(hModule, "GetDiskFreeSpaceExA") != NULL;
        CResCache::pfnGetDiskFreeSpaceExA = reinterpret_cast<GetDiskFreeSpaceExAFunc*>(GetProcAddress(hModule, "GetDiskFreeSpaceExA"));

        if (!bHaveGetDiskFreeSpaceExA
            || !CResCache::pfnGetDiskFreeSpaceExA(NULL, &nFreeBytesAvailable, &nTotalNumberOfBytes, NULL)
            || (nFreeBytesAvailable.HighPart == 0 && nFreeBytesAvailable.LowPart < CBaldurChitin::MINIMUM_DRIVE_SPACE)) {
            if (bProgressBarInPlace) {
                g_pChitin->SetProgressBar(FALSE, 0, 0, 0, FALSE, 0, FALSE, 0, FALSE, FALSE, 255);
            }

            g_pBaldurChitin->m_dwCloseConfirmationStrId = 11790;
            g_pBaldurChitin->m_dwCloseConfirmationFlags = 0x34;
            PostMessageA(g_pBaldurChitin->cWnd.GetSafeHwnd(), WM_CLOSE, 0, 0);
            FreeLibrary(hModule);
            return FALSE;
        }

        FreeLibrary(hModule);
    }

    if (!bProgressBarInPlace && bProgressBarRequired == TRUE) {
        sub_59FA00(TRUE);
        g_pChitin->SetProgressBar(TRUE, 9890, 0, 0, FALSE, 0, FALSE, 0, FALSE, FALSE, 255);
        g_pChitin->cProgressBar.m_nActionProgress = 0;
        g_pChitin->cProgressBar.m_nActionTarget = 5000000;
        g_pChitin->cProgressBar.m_bDisableMinibars = TRUE;
    }

    // The moves still pending go into the save as timed effects.
    m_cMoveList.Marshal(g_pBaldurChitin->GetObjectGame()->m_pGameAreaMaster);
    m_cLimboList.Marshal(g_pBaldurChitin->GetObjectGame()->m_pGameAreaMaster);

    if (bProgressBarInPlace || bProgressBarRequired) {
        ProgressBarCallback(0, TRUE);
        g_pChitin->cProgressBar.AddActionTarget(2500000);

        // NOTE: The original also requires the multiplayer screen's startup
        // flag to be clear. That screen is gone (ADR 0002); its flag is clear
        // in single player.
        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
            g_pChitin->cProgressBar.AddActionTarget(6000000);
        }

        for (cnt = 0; cnt < CINFGAME_MAX_AREAS; cnt++) {
            if (m_gameAreas[cnt] != NULL && (m_gameAreas[cnt]->m_header.m_flags & 0x1) == 0) {
                g_pChitin->cProgressBar.AddActionTarget(3000000);
            }
        }
    }

    // NOTE: See above for the multiplayer screen's startup flag.
    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
        g_pBaldurChitin->m_pEngineWorld->StartSaveGameMultiplayerHost();
        g_pBaldurChitin->GetBaldurMessage()->UpdateDemandCharacters(TRUE, 0, bProgressBarRequired | bProgressBarInPlace);
        g_pBaldurChitin->GetBaldurMessage()->PurgeDroppedPlayers();
        g_pBaldurChitin->GetBaldurMessage()->BroadcastUpdateCharacterSlot(TRUE, 0, bProgressBarRequired | bProgressBarInPlace);
        g_pBaldurChitin->GetBaldurMessage()->RequestClientSignal(CBaldurMessage::SIGNAL_END_MAJOR_EVENT);
    }

    if (bSaveScreen) {
        field_366E = 1;
    }

    // NOTE: The original passes each of these ids to a CBaldurMessage
    // helper (0x43D8D0) that hands an object held by another player to that
    // player's machine and waits for it. It returns at once when there is
    // no session or the provider is the null one (0x43D8F3, 0x43DA22), so a
    // single-player host session never sends; it needs a remote player, so
    // the helper is not ported and the ids are fetched and dropped.
    std::vector<int> ids = Icewind586B70::Instance()->sub_587510();
    (void)ids;

    Icewind586B70::Instance()->sub_587190();

    g_pBaldurChitin->GetActiveEngine()->GetManager()->InvalidateRect(NULL);
    g_pBaldurChitin->m_cCachingStatus.InvalidateScreen();

    if (bProgressBarInPlace) {
        ProgressBarCallback(312500, FALSE);
        ProgressBarCallback(312500, FALSE);
    } else if (bProgressBarRequired) {
        ProgressBarCallback(312500, FALSE);
    }

    for (cnt = 0; cnt < 12; cnt++) {
        if (m_aServerStore[cnt] != NULL) {
            m_aServerStore[cnt]->Marshal(m_sTempDir);
        }
    }

    BOOLEAN bProgressBar = bProgressBarRequired | bProgressBarInPlace;

    pData = NULL;
    Marshal(&pData, &nData, bProgressBar);
    if (pData != NULL) {
        // NOTE: Uninline.
        cGameFile.SetResRef(CResRef("ICEWIND2"), TRUE, TRUE);
        static_cast<CRes*>(cGameFile.GetRes())->Write(m_sTempDir, pData, nData);
        delete[] pData;
    } else {
        bResult = FALSE;
    }

    for (cnt = 0; cnt < CINFGAME_MAX_AREAS; cnt++) {
        if (m_gameAreas[cnt] != NULL && (m_gameAreas[cnt]->m_header.m_flags & 0x1) == 0) {
            BYTE* pAreaData = NULL;
            DWORD nAreaData = 0;
            m_gameAreas[cnt]->Marshal(&pAreaData, &nAreaData, bProgressBar);
            if (pAreaData != NULL) {
                CAreaFile cAreaFile;

                // NOTE: Uninline.
                cAreaFile.SetResRef(m_gameAreas[cnt]->m_resRef, TRUE, TRUE);
                static_cast<CRes*>(cAreaFile.GetRes())->Write(m_sTempDir, pAreaData, nAreaData);
                delete[] pAreaData;
            } else {
                bResult = FALSE;
            }
        }
    }

    m_cWorldMap.Marshal(m_sTempDir);

    g_pBaldurChitin->GetTlkTable().m_override.Save();
    g_pBaldurChitin->GetTlkTable().m_override.CloseFiles();

    if (bResult) {
        if (!g_pChitin->cDimm.DirectoryCopyFiles(m_sTempDir, m_sTempSaveDir)) {
            bResult = FALSE;
        } else if (!g_pChitin->cDimm.CompressDirectory(m_sTempSaveDir, m_sTempSaveDir)) {
            // The area files are packed into ICEWIND2.SAV (0x784A50), which
            // LoadGame's UncompressDirectory unpacks into temp again.
            bResult = FALSE;
        } else {
            if (!g_pChitin->cDimm.DirectoryRemoveFiles(GetDirSave())
                || !g_pChitin->cDimm.DirectoryCopyFiles(m_sTempSaveDir, GetDirSave())) {
                bResult = FALSE;
            }
        }
    }

    g_pChitin->cDimm.DirectoryRemoveFiles(m_sTempSaveDir);

    g_pBaldurChitin->GetTlkTable().m_override.Open(CString("temp//default.toh"), CString("temp//default.tot"));

    if (bProgressBarInPlace || bProgressBarRequired) {
        ProgressBarCallback(312500, FALSE);
    }

    EnterCriticalSection(&(g_pChitin->field_314));
    g_pChitin->cDimm.m_cKeyTable.RescanEverything();
    LeaveCriticalSection(&(g_pChitin->field_314));

    if (bProgressBarInPlace || bProgressBarRequired) {
        ProgressBarCallback(312500, FALSE);
    }

    SaveOptions();
    g_pBaldurChitin->SaveOptions();

    m_nLastSaveTime = m_worldTime.m_gameTime;

    // NOTE: See above for the multiplayer screen's startup flag.
    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
        if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
            g_pChitin->cProgressBar.m_nWaitingReason = 19714;
            g_pChitin->cProgressBar.m_bWaiting = TRUE;
        }

        while (g_pBaldurChitin->m_pEngineWorld->m_bHardPaused == TRUE
            && g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
            g_pBaldurChitin->m_pEngineWorld->CheckEndOfHardPause();
            g_pBaldurChitin->GetBaldurMessage()->HandleBlockingMessages();
            g_pChitin->m_bDisplayStale = TRUE;

            if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL) {
                SynchronousUpdate();
            } else {
                SleepEx(60, FALSE);
            }
        }

        if (!g_pChitin->cNetwork.GetSessionOpen()) {
            if (bProgressBarInPlace || bProgressBarRequired) {
                g_pChitin->SetProgressBar(FALSE, 0, 0, 0, FALSE, 0, FALSE, 0, FALSE, FALSE, 255);
            }

            return TRUE;
        }

        g_pChitin->cProgressBar.m_bWaiting = FALSE;
        g_pBaldurChitin->m_cCachingStatus.InvalidateScreen();
    }

    if (bProgressBarInPlace || bProgressBarRequired) {
        ProgressBarCallback(1250000, FALSE);
    }

    if (!bProgressBarInPlace && bProgressBarRequired == TRUE) {
        g_pChitin->cProgressBar.m_nActionProgress = g_pChitin->cProgressBar.m_nActionTarget;
        g_pChitin->cProgressBar.m_bDisableMinibars = TRUE;
        g_pChitin->m_bDisplayStale = TRUE;
        sub_59FA00(TRUE);
        g_pChitin->SetProgressBar(FALSE, 0, 0, 0, FALSE, 0, FALSE, 0, FALSE, FALSE, 255);
    }

    if (bResult) {
        if (GetDiskFreeSpaceA(NULL, &dwSectorsPerCluster, &dwBytesPerSector, &dwNumberOfFreeClusters, &dwTotalNumberOfClusters) != TRUE
            || dwSectorsPerCluster * dwNumberOfFreeClusters < CBaldurChitin::MINIMUM_DRIVE_SECTORS
            || dwSectorsPerCluster * dwNumberOfFreeClusters < CBaldurChitin::MINIMUM_DRIVE_SPACE / dwBytesPerSector) {
            hModule = LoadLibraryA("kernel32.dll");
            if (hModule == NULL) {
                g_pBaldurChitin->m_dwCloseConfirmationStrId = 10249;
                g_pBaldurChitin->m_dwCloseConfirmationFlags = 0x10;
                PostMessageA(g_pBaldurChitin->cWnd.GetSafeHwnd(), WM_CLOSE, 0, 0);
                return FALSE;
            }

            BOOL bHaveGetDiskFreeSpaceExA = GetProcAddress(hModule, "GetDiskFreeSpaceExA") != NULL;
            CResCache::pfnGetDiskFreeSpaceExA = reinterpret_cast<GetDiskFreeSpaceExAFunc*>(GetProcAddress(hModule, "GetDiskFreeSpaceExA"));

            if (!bHaveGetDiskFreeSpaceExA
                || !CResCache::pfnGetDiskFreeSpaceExA(NULL, &nFreeBytesAvailable, &nTotalNumberOfBytes, NULL)
                || (nFreeBytesAvailable.HighPart == 0 && nFreeBytesAvailable.LowPart < CBaldurChitin::MINIMUM_DRIVE_SPACE)) {
                g_pBaldurChitin->m_dwCloseConfirmationStrId = 10249;
                g_pBaldurChitin->m_dwCloseConfirmationFlags = 0x10;
                PostMessageA(g_pBaldurChitin->cWnd.GetSafeHwnd(), WM_CLOSE, 0, 0);
                FreeLibrary(hModule);
                return FALSE;
            }

            FreeLibrary(hModule);
        }
    } else {
        g_pBaldurChitin->m_dwCloseConfirmationStrId = 23554;
        g_pBaldurChitin->m_dwCloseConfirmationFlags = 0x34;
        PostMessageA(g_pBaldurChitin->cWnd.GetSafeHwnd(), WM_CLOSE, 0, 0);
    }

    return TRUE;
}

// 0x5A3230
void CInfGame::Marshal(BYTE** pGame, DWORD* nGame, BOOLEAN bProgressBarInPlace)
{
    CSavedGamePartyCreature partyCreatures[6];
    CSavedGamePartyCreature* pGlobalCreature;
    CVariable* pVariables;
    WORD nVariables;
    DWORD dwProgressPerCreature;
    DWORD dwProgressFinal;
    SHORT nVisibleAreaCharacter;
    SHORT cnt3;
    SHORT nCharacterSlot;
    CGameSprite* pSprite;
    DWORD cnt;
    INT nIndex;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 4924
    UTIL_ASSERT(pGame != NULL && nGame != NULL);

    memset(partyCreatures, 0, sizeof(partyCreatures));

    pGlobalCreature = NULL;
    pVariables = NULL;
    nVisibleAreaCharacter = 0;

    if (bProgressBarInPlace) {
        ProgressBarCallback(0, TRUE);

        if (m_nCharacters + m_lstGlobalCreatures.GetCount() != 0) {
            dwProgressPerCreature = 2187500 / (m_nCharacters + m_lstGlobalCreatures.GetCount());
            dwProgressFinal = 312500;
        } else {
            dwProgressPerCreature = 0;
            dwProgressFinal = 2500000;
        }
    }

    memset(partyCreatures, 0, sizeof(partyCreatures));

    *nGame = sizeof(CSavedGameHeader) + 8;
    *nGame = sizeof(CSavedGameHeader) + 8 + m_nCharacters * sizeof(CSavedGamePartyCreature);

    cnt3 = 0;
    for (nCharacterSlot = 0; nCharacterSlot < 6; nCharacterSlot++) {
        if (m_characters[nCharacterSlot] != CGameObjectArray::INVALID_INDEX) {
            if (bProgressBarInPlace) {
                ProgressBarCallback(dwProgressPerCreature, FALSE);
            }

            if (m_cObjectArray.GetShare(m_characters[nCharacterSlot],
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE)
                == CGameObjectArray::SUCCESS) {
                pSprite->Marshal(partyCreatures[cnt3], FALSE);
                *nGame += partyCreatures[cnt3].m_creatureSize;

                if (pSprite->GetArea() == m_gameAreas[m_visibleArea]) {
                    nVisibleAreaCharacter = nCharacterSlot;
                }

                m_cObjectArray.ReleaseShare(m_characters[nCharacterSlot],
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);

                cnt3++;
            } else {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
                // __LINE__: 4979
                UTIL_ASSERT(FALSE);
            }
        }
    }

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 4990
    UTIL_ASSERT(cnt3 == m_nCharacters);

    if (m_lstGlobalCreatures.GetCount() > 0) {
        pGlobalCreature = new CSavedGamePartyCreature[m_lstGlobalCreatures.GetCount()];

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 4995
        UTIL_ASSERT(pGlobalCreature != NULL);

        memset(pGlobalCreature, 0, sizeof(CSavedGamePartyCreature) * m_lstGlobalCreatures.GetCount());

        *nGame += sizeof(CSavedGamePartyCreature) * m_lstGlobalCreatures.GetCount();

        POSITION pos = m_lstGlobalCreatures.GetHeadPosition();
        nIndex = 0;
        while (pos != NULL) {
            LONG nCharacterId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lstGlobalCreatures.GetNext(pos)));

            if (bProgressBarInPlace) {
                ProgressBarCallback(dwProgressPerCreature, FALSE);
            }

            if (m_cObjectArray.GetShare(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE)
                == CGameObjectArray::SUCCESS) {
                pSprite->Marshal(pGlobalCreature[static_cast<SHORT>(nIndex)], FALSE);
                *nGame += pGlobalCreature[static_cast<SHORT>(nIndex)].m_creatureSize;

                m_cObjectArray.ReleaseShare(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }

            nIndex++;
        }
    }

    m_variables.Marshal(&pVariables, &nVariables);
    *nGame += sizeof(CVariable) * nVariables;

    *nGame += sizeof(CSavedGameJournalEntry) * m_cJournal.CountEntries() + 400;

    for (INT nAlignment = 0; nAlignment < 9; nAlignment++) {
        for (INT nLevel = 0; nLevel < 9; nLevel++) {
            *nGame += RESREF_SIZE * m_familiarResRefs[nAlignment][nLevel].GetCount();
        }
    }

    if (bProgressBarInPlace) {
        ProgressBarCallback(dwProgressFinal, FALSE);
    }

    DWORD nAreaLevels = 4;
    POSITION posAreaLevel = field_4BDC.GetHeadPosition();
    while (posAreaLevel != NULL) {
        CInfGameAreaLevel* pAreaLevel = field_4BDC.GetNext(posAreaLevel);
        nAreaLevels += pAreaLevel->m_sArea.GetLength() + 28;
    }

    *nGame += nAreaLevels;

    *pGame = new BYTE[*nGame];

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 5063
    UTIL_ASSERT(*pGame != NULL);

    memset(*pGame, 0, *nGame);

    memcpy(*pGame, "GAMEV2.2", 8);
    cnt = 8;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 5067
    UTIL_ASSERT(cnt <= (DWORD)*nGame);

    CSavedGameHeader* pHeader = reinterpret_cast<CSavedGameHeader*>(*pGame + cnt);
    cnt += sizeof(CSavedGameHeader);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 5070
    UTIL_ASSERT(cnt <= (DWORD)*nGame);

    pHeader->m_worldTime = m_worldTime.m_gameTime / CTimerWorld::TIMESCALE_MSEC_PER_SEC;
    m_gameSave.m_cResCurrentWorldArea.GetResRef(pHeader->m_currentWorldArea);
    pHeader->m_currentLink = m_gameSave.m_nCurrentWorldLink;
    pHeader->m_nPCAreaViewed = nVisibleAreaCharacter;

    pHeader->m_curFormation = 0;
    for (SHORT nFormation = CGAMESAVE_NUM_QUICK_FORMATIONS - 1; nFormation >= 0; nFormation--) {
        pHeader->m_quickFormations[nFormation] = m_gameSave.m_quickFormations[nFormation];
        if (m_gameSave.m_curFormation == m_gameSave.m_quickFormations[nFormation]) {
            pHeader->m_curFormation = nFormation;
        }
    }

    pHeader->m_nPartyGold = m_gameSave.m_nPartyGold;
    pHeader->m_wWeatherFlags = g_pBaldurChitin->m_pEngineWorld->m_weather.Marshal();

    if (!m_bPartyAI) {
        pHeader->m_dwFlags |= 0x1;
    }

    if (!m_bShowAreaNotes) {
        pHeader->m_dwFlags |= 0x80;
    }

    if (g_pBaldurChitin->m_pEngineWorld->field_11F0 == 0) {
        pHeader->m_dwFlags |= 0x20;
    }

    if (g_pBaldurChitin->m_pEngineWorld->field_11F4 == 0) {
        pHeader->m_dwFlags |= 0x40;
    }

    if (g_pBaldurChitin->m_pEngineWorld->GetManager()->m_bHidden) {
        pHeader->m_dwFlags |= 0x10;
    }

    pHeader->m_versionNumber = 3;

    BYTE nPanelIds[2];
    BYTE nControlIds[2];
    nPanelIds[0] = 7;
    nPanelIds[1] = 19;
    nControlIds[0] = 0;
    nControlIds[1] = 0;

    for (BYTE nPanel = 0; nPanel < 2; nPanel++) {
        CUIPanel* pPanel = g_pBaldurChitin->m_pEngineWorld->GetManager()->GetPanel(nPanelIds[nPanel]);
        if (pPanel->m_bActive) {
            if (pPanel->GetControl(nControlIds[nPanel])->m_bActive) {
                pHeader->m_dwFlags |= 0x2 | 0x4;
            }
            break;
        }
    }

    pHeader->m_reputation = m_nReputation;

    if (m_pGameAreaMaster != NULL) {
        m_pGameAreaMaster->m_resRef.GetResRef(pHeader->m_masterArea);
    }

    pHeader->m_partyCreatureTableOffset = sizeof(CSavedGameHeader) + 8;
    pHeader->m_partyCreatureTableCount = m_nCharacters;

    cnt = sizeof(CSavedGameHeader) + 8 + m_nCharacters * sizeof(CSavedGamePartyCreature);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 5136
    UTIL_ASSERT(cnt <= (DWORD)*nGame);

    cnt3 = 0;
    for (nCharacterSlot = 0; nCharacterSlot < 6; nCharacterSlot++) {
        if (m_characters[nCharacterSlot] != CGameObjectArray::INVALID_INDEX) {
            if (bProgressBarInPlace) {
                ProgressBarCallback(dwProgressPerCreature, FALSE);
            }

            CSavedGamePartyCreature* pPartyCreature = reinterpret_cast<CSavedGamePartyCreature*>(*pGame + pHeader->m_partyCreatureTableOffset) + cnt3;
            memcpy(pPartyCreature, &(partyCreatures[cnt3]), sizeof(CSavedGamePartyCreature));

            if (partyCreatures[cnt3].m_creatureSize != 0) {
                // NOTE: The marshal stores a 32-bit handle (compat/marshal_ptr.h)
                // where the original stores the pointer. A handle the table does
                // not know gives NULL, and the bytes then stay zero.
                BYTE* pCreatureData = static_cast<BYTE*>(iwd2r::UnmarshalPtr(partyCreatures[cnt3].m_creatureOffset));
                if (pCreatureData != NULL) {
                    memcpy(*pGame + cnt, pCreatureData, partyCreatures[cnt3].m_creatureSize);
                }
                delete[] pCreatureData;

                pPartyCreature->m_creatureOffset = cnt;
                cnt += partyCreatures[cnt3].m_creatureSize;

                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
                // __LINE__: 5156
                UTIL_ASSERT(cnt <= (DWORD)*nGame);
            }

            cnt3++;
        }
    }

    pHeader->m_globalCreatureTableOffset = cnt;
    pHeader->m_globalCreatureTableCount = m_lstGlobalCreatures.GetCount();

    cnt += sizeof(CSavedGamePartyCreature) * m_lstGlobalCreatures.GetCount();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 5166
    UTIL_ASSERT(cnt <= (DWORD)*nGame);

    for (SHORT nGlobal = 0; nGlobal < m_lstGlobalCreatures.GetCount(); nGlobal++) {
        if (bProgressBarInPlace) {
            ProgressBarCallback(dwProgressPerCreature, FALSE);
        }

        CSavedGamePartyCreature* pSavedCreature = reinterpret_cast<CSavedGamePartyCreature*>(*pGame + pHeader->m_globalCreatureTableOffset) + nGlobal;
        memcpy(pSavedCreature, &(pGlobalCreature[nGlobal]), sizeof(CSavedGamePartyCreature));

        if (pGlobalCreature[nGlobal].m_creatureSize != 0) {
            // NOTE: The marshal stores a 32-bit handle (compat/marshal_ptr.h)
            // where the original stores the pointer. A handle the table does
            // not know gives NULL, and the bytes then stay zero.
            BYTE* pCreatureData = static_cast<BYTE*>(iwd2r::UnmarshalPtr(pGlobalCreature[nGlobal].m_creatureOffset));
            if (pCreatureData != NULL) {
                memcpy(*pGame + cnt, pCreatureData, pGlobalCreature[nGlobal].m_creatureSize);
            }
            delete[] pCreatureData;

            pSavedCreature->m_creatureOffset = cnt;
            cnt += pGlobalCreature[nGlobal].m_creatureSize;

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 5183
            UTIL_ASSERT(cnt <= (DWORD)*nGame);
        }
    }

    if (pGlobalCreature != NULL) {
        delete[] pGlobalCreature;
    }

    if (nVariables > 0) {
        pHeader->m_globalVariablesOffset = cnt;
        pHeader->m_globalVariablesCount = nVariables;
        memcpy(*pGame + cnt, pVariables, sizeof(CVariable) * nVariables);
        delete[] pVariables;

        cnt += sizeof(CVariable) * nVariables;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 5197
        UTIL_ASSERT(cnt <= (DWORD)*nGame);
    }

    pHeader->m_journalEntriesOffset = cnt;
    pHeader->m_journalEntriesCount = m_cJournal.CountEntries();
    m_cJournal.Marshal(reinterpret_cast<CSavedGameJournalEntry*>(*pGame + cnt));
    cnt += sizeof(CSavedGameJournalEntry) * pHeader->m_journalEntriesCount;

    // The dword at 0x6C of the file (0x64 of the header) holds the
    // nightmare mode.
    pHeader->m_storedLocationsOffset = m_cOptions.m_nNightmareMode;

    pHeader->m_familiarsOffset = cnt;

    DWORD* pFamiliars = reinterpret_cast<DWORD*>(*pGame + cnt);
    cnt += 400;
    memcpy(pFamiliars, m_defaultFamiliarResRefs, sizeof(m_defaultFamiliarResRefs));
    pFamiliars[18] = cnt;

    DWORD* pFamiliarCounts = pFamiliars + 19;
    for (INT nAlignment = 0; nAlignment < 9; nAlignment++) {
        for (INT nLevel = 0; nLevel < 9; nLevel++) {
            *pFamiliarCounts = m_familiarResRefs[nAlignment][nLevel].GetCount();

            POSITION pos = m_familiarResRefs[nAlignment][nLevel].GetHeadPosition();
            while (pos != NULL) {
                CResRef* pResRef = m_familiarResRefs[nAlignment][nLevel].GetNext(pos);
                pResRef->GetResRef(*pGame + cnt);
                cnt += RESREF_SIZE;
            }

            pFamiliarCounts++;
        }
    }

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 5225
    UTIL_ASSERT(cnt <= (DWORD)*nGame);

    BYTE* pAreaLevels = *pGame + cnt;
    *reinterpret_cast<DWORD*>(pAreaLevels) = field_4BDC.GetCount();
    pAreaLevels += 4;

    posAreaLevel = field_4BDC.GetHeadPosition();
    while (posAreaLevel != NULL) {
        CInfGameAreaLevel* pAreaLevel = field_4BDC.GetNext(posAreaLevel);
        DWORD nLength = pAreaLevel->m_sArea.GetLength();
        *reinterpret_cast<DWORD*>(pAreaLevels) = nLength + 24;
        *reinterpret_cast<DWORD*>(pAreaLevels + 4) = nLength;
        *reinterpret_cast<DWORD*>(pAreaLevels + 8) = pAreaLevel->m_dwFlags;
        *reinterpret_cast<DWORD*>(pAreaLevels + 12) = pAreaLevel->m_nLevel;
        memcpy(pAreaLevels + 13, pAreaLevel->field_9, sizeof(pAreaLevel->field_9));
        memcpy(pAreaLevels + 28, static_cast<LPCSTR>(pAreaLevel->m_sArea), nLength);
        pAreaLevels += nLength + 28;
    }

    nAreaLevels = 4;
    posAreaLevel = field_4BDC.GetHeadPosition();
    while (posAreaLevel != NULL) {
        CInfGameAreaLevel* pAreaLevel = field_4BDC.GetNext(posAreaLevel);
        nAreaLevels += pAreaLevel->m_sArea.GetLength() + 28;
    }

    cnt += nAreaLevels;

    if (bProgressBarInPlace) {
        ProgressBarCallback(dwProgressFinal, FALSE);
    }

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 5238
    UTIL_ASSERT(cnt == (DWORD)*nGame);
}

// NOTE: Converts a version 2.0 party creature (0x180 bytes) to a version 2.1
// one (0x220 bytes), field by field. A free function in the original.
//
// 0x5A79E0
static void sub_5A79E0(const BYTE* pSrc, BYTE* pDst)
{
    memcpy(pDst + 0x0, pSrc + 0x0, 2);
    memcpy(pDst + 0x2, pSrc + 0x2, 2);
    memcpy(pDst + 0x4, pSrc + 0x4, 4);
    memcpy(pDst + 0x8, pSrc + 0x8, 4);
    memcpy(pDst + 0xC, pSrc + 0xC, 4);
    memcpy(pDst + 0x10, pSrc + 0x10, 4);
    memcpy(pDst + 0x14, pSrc + 0x14, 4);
    memcpy(pDst + 0x18, pSrc + 0x18, 4);
    memcpy(pDst + 0x1C, pSrc + 0x1C, 4);
    memcpy(pDst + 0x20, pSrc + 0x20, 2);
    memcpy(pDst + 0x22, pSrc + 0x22, 2);
    memcpy(pDst + 0x24, pSrc + 0x24, 2);
    memcpy(pDst + 0x26, pSrc + 0x26, 2);
    memcpy(pDst + 0x28, pSrc + 0x28, 2);
    memcpy(pDst + 0x2A, pSrc + 0x2A, 2);
    memcpy(pDst + 0x2C, pSrc + 0x2C, 96);
    memcpy(pDst + 0x8C, pSrc + 0x8C, 4);
    memcpy(pDst + 0x90, pSrc + 0x90, 4);
    memcpy(pDst + 0x9C, pSrc + 0x94, 4);
    memcpy(pDst + 0xA0, pSrc + 0x98, 4);
    memcpy(pDst + 0xAC, pSrc + 0x9C, 24);
    memcpy(pDst + 0xDC, pSrc + 0xB4, 4);
    memcpy(pDst + 0xE0, pSrc + 0xB8, 2);
    memcpy(pDst + 0xE2, pSrc + 0xBA, 4);
    memcpy(pDst + 0xE6, pSrc + 0xBE, 2);
    memcpy(pDst + 0x160, pSrc + 0xC0, 32);
    memcpy(pDst + 0x180, pSrc + 0xE0, 4);
    memcpy(pDst + 0x184, pSrc + 0xE4, 4);
    memcpy(pDst + 0x188, pSrc + 0xE8, 4);
    memcpy(pDst + 0x18C, pSrc + 0xEC, 4);
    memcpy(pDst + 0x190, pSrc + 0xF0, 4);
    memcpy(pDst + 0x194, pSrc + 0xF4, 4);
    memcpy(pDst + 0x198, pSrc + 0xF8, 4);
    memcpy(pDst + 0x19C, pSrc + 0xFC, 4);
    memcpy(pDst + 0x1A0, pSrc + 0x100, 4);
    memcpy(pDst + 0x1A4, pSrc + 0x104, 4);
    memcpy(pDst + 0x1A8, pSrc + 0x108, 32);
    memcpy(pDst + 0x1C8, pSrc + 0x128, 4);
    memcpy(pDst + 0x1CC, pSrc + 0x12C, 4);
    memcpy(pDst + 0x1D0, pSrc + 0x130, 32);
    memcpy(pDst + 0x1F0, pSrc + 0x150, 4);
    memcpy(pDst + 0x1F4, pSrc + 0x154, 4);
    memcpy(pDst + 0x1F8, pSrc + 0x158, 4);
    memcpy(pDst + 0x1FC, pSrc + 0x15C, 4);
    memcpy(pDst + 0x200, pSrc + 0x160, 32);
}

// NOTE: Converts a version 2.1 party creature (0x220 bytes) to a version 2.2
// one (0x340 bytes, CSavedGamePartyCreature), field by field. A free function
// in the original.
//
// 0x5A7BF0
static void sub_5A7BF0(const BYTE* pSrc, BYTE* pDst)
{
    memcpy(pDst + 0x0, pSrc + 0x0, 2);
    memcpy(pDst + 0x2, pSrc + 0x2, 2);
    memcpy(pDst + 0x4, pSrc + 0x4, 4);
    memcpy(pDst + 0x8, pSrc + 0x8, 4);
    memcpy(pDst + 0xC, pSrc + 0xC, 4);
    memcpy(pDst + 0x10, pSrc + 0x10, 4);
    memcpy(pDst + 0x14, pSrc + 0x14, 4);
    memcpy(pDst + 0x18, pSrc + 0x18, 4);
    memcpy(pDst + 0x1C, pSrc + 0x1C, 4);
    memcpy(pDst + 0x20, pSrc + 0x20, 2);
    memcpy(pDst + 0x22, pSrc + 0x22, 2);
    memcpy(pDst + 0x24, pSrc + 0x24, 2);
    memcpy(pDst + 0x26, pSrc + 0x26, 2);
    memcpy(pDst + 0x28, pSrc + 0x28, 2);
    memcpy(pDst + 0x2A, pSrc + 0x2A, 2);
    memcpy(pDst + 0x2C, pSrc + 0x2C, 96);
    memcpy(pDst + 0x8C, pSrc + 0x8C, 4);
    memcpy(pDst + 0x90, pSrc + 0x90, 4);
    memcpy(pDst + 0x94, pSrc + 0x94, 4);
    memcpy(pDst + 0x98, pSrc + 0x98, 4);
    memcpy(pDst + 0x9C, pSrc + 0x9C, 4);
    memcpy(pDst + 0xA0, pSrc + 0xA0, 4);
    memcpy(pDst + 0xA4, pSrc + 0xA4, 4);
    memcpy(pDst + 0xA8, pSrc + 0xA8, 4);
    memcpy(pDst + 0xAC, pSrc + 0xAC, 48);
    memcpy(pDst + 0xFE, pSrc + 0xDC, 4);
    memcpy(pDst + 0x102, pSrc + 0xE0, 2);
    memcpy(pDst + 0x104, pSrc + 0xE2, 4);
    memcpy(pDst + 0x108, pSrc + 0xE6, 2);
    memcpy(pDst + 0x10A, pSrc + 0xE8, 48);
    memcpy(pDst + 0x152, pSrc + 0x118, 48);
    memcpy(pDst + 0x19A, pSrc + 0x148, 24);
    memcpy(pDst + 0x1BE, pSrc + 0x160, 32);
    memcpy(pDst + 0x1DE, pSrc + 0x180, 4);
    memcpy(pDst + 0x1E2, pSrc + 0x184, 4);
    memcpy(pDst + 0x1E6, pSrc + 0x188, 4);
    memcpy(pDst + 0x1EA, pSrc + 0x18C, 4);
    memcpy(pDst + 0x1EE, pSrc + 0x190, 4);
    memcpy(pDst + 0x1F2, pSrc + 0x194, 4);
    memcpy(pDst + 0x1F6, pSrc + 0x198, 4);
    memcpy(pDst + 0x1FA, pSrc + 0x19C, 4);
    memcpy(pDst + 0x1FE, pSrc + 0x1A0, 4);
    memcpy(pDst + 0x202, pSrc + 0x1A4, 4);
    memcpy(pDst + 0x206, pSrc + 0x1A8, 32);
    memcpy(pDst + 0x226, pSrc + 0x1C8, 4);
    memcpy(pDst + 0x22A, pSrc + 0x1CC, 4);
    memcpy(pDst + 0x22E, pSrc + 0x1D0, 32);
    memcpy(pDst + 0x24E, pSrc + 0x1F0, 4);
    memcpy(pDst + 0x252, pSrc + 0x1F4, 4);
    memcpy(pDst + 0x256, pSrc + 0x1F8, 4);
    memcpy(pDst + 0x25A, pSrc + 0x1FC, 4);
    memcpy(pDst + 0x25E, pSrc + 0x200, 32);
}

// 0x5A7E40
void CInfGame::Unmarshal(BYTE* pGame, LONG nGame, BOOLEAN bProgressBarInPlace)
{
    CBaldurMessage* pMessage;
    BYTE pCreature20[0x220];
    BYTE pCreature21[sizeof(CSavedGamePartyCreature)];
    CSavedGamePartyCreature* pCreature22;
    CCreatureFile creatureData;
    INT nVersion;
    DWORD cnt;
    DWORD dwProgressPerCreature;
    DWORD dwProgressFinal;
    CGameSprite* pSprite;
    INT nMsgFrom;
    DWORD dwSize;
    BYTE* pData;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 7006
    UTIL_ASSERT(pGame != NULL && nGame > 0);

    pMessage = g_pBaldurChitin->GetBaldurMessage();

    memset(pCreature20, 0, sizeof(pCreature20));
    memset(pCreature21, 0, sizeof(pCreature21));

    pCreature22 = NULL;

    if (memcmp(pGame, "GAMEV2.1", 8) == 0) {
        nVersion = 21;
    } else if (memcmp(pGame, "GAMEV2.2", 8) == 0) {
        nVersion = 22;
    } else {
        nVersion = 20;
    }

    CString sFileType(reinterpret_cast<char*>(pGame), 4);
    CString sFileVersion(reinterpret_cast<char*>(pGame + 4), 4);

    cnt = 8;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 7042
    UTIL_ASSERT(cnt < (DWORD)nGame);

    CSavedGameHeader* pHeader = reinterpret_cast<CSavedGameHeader*>(pGame + cnt);
    cnt += sizeof(CSavedGameHeader);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 7045
    UTIL_ASSERT(cnt < (DWORD)nGame);

    m_worldTime.m_gameTime = CTimerWorld::TIMESCALE_MSEC_PER_SEC * pHeader->m_worldTime;
    m_worldTime.CheckForTriggerEventPast();

    if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
        || g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
        m_gameSave.m_curFormation = pHeader->m_quickFormations[pHeader->m_curFormation];
    }

    m_gameSave.m_cResCurrentWorldArea = pHeader->m_currentWorldArea;
    m_gameSave.m_nCurrentWorldLink = pHeader->m_currentLink;

    if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
        || g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
        for (INT nFormation = 0; nFormation < CGAMESAVE_NUM_QUICK_FORMATIONS; nFormation++) {
            m_gameSave.m_quickFormations[nFormation] = pHeader->m_quickFormations[nFormation];
        }
    }

    m_gameSave.m_nPartyGold = pHeader->m_nPartyGold;

    m_cOptions.m_nNightmareMode = pHeader->m_storedLocationsOffset;
    if (m_cOptions.m_nNightmareMode == 1 && m_cOptions.m_nDifficultyLevel != 5) {
        m_cOptions.m_nDifficultyLevel = 5;

        if (g_pBaldurChitin->cNetwork.GetSessionHosting() == TRUE) {
            m_cOptions.m_nDifficultyMultiplier = 100;
            m_cOptions.m_nMPDifficultyMultiplier = 100;
        }

        CString sDifficultyLevel;
        sDifficultyLevel.Format("%d", 5);
        WritePrivateProfileStringA("Game Options",
            "Difficulty Level",
            sDifficultyLevel,
            g_pBaldurChitin->GetIniFileName());
    }

    m_bShowAreaNotes = ((pHeader->m_dwFlags >> 7) & 1) == 0;
    m_nReputation = static_cast<SHORT>(pHeader->m_reputation);

    if (bProgressBarInPlace) {
        ProgressBarCallback(0, TRUE);

        DWORD nCreatures = pHeader->m_partyCreatureTableCount + pHeader->m_globalCreatureTableCount;

        if (pHeader->m_partyCreatureTableCount > 1) {
            g_pChitin->cProgressBar.AddActionTarget(3000000 * pHeader->m_partyCreatureTableCount - 3000000);
        }

        if (nCreatures > 0) {
            dwProgressPerCreature = 4374000 / nCreatures;
            dwProgressFinal = 156250;
        } else {
            dwProgressPerCreature = 0;
            dwProgressFinal = 1250000;
        }

        ProgressBarCallback(dwProgressFinal, FALSE);
    }

    if (pHeader->m_masterArea[0] != '\0' && bProgressBarInPlace) {
        g_pChitin->cProgressBar.AddActionTarget(3000000);
    }

    g_pBaldurChitin->m_pEngineWorld->m_weather.Unmarshal(pHeader->m_wWeatherFlags);

    // NOTE: The original never advances this loop: it reads the first entry
    // of the party table (not the global creature table) over and over while
    // the global creature count is nonzero, so a game file with global
    // creatures never finishes loading. It also reads the entry's resref and
    // data through `pGlobalEntry` and unmarshals through `pCreature22`, and
    // for some versions one of them is still NULL. The original's game files
    // have no global creatures. Kept as it is.
    CSavedGamePartyCreature* pGlobalEntry = NULL;
    while (pHeader->m_globalCreatureTableCount > 0) {
        if (nVersion == 20) {
            sub_5A79E0(pGame + pHeader->m_partyCreatureTableOffset, pCreature20);
            pGlobalEntry = reinterpret_cast<CSavedGamePartyCreature*>(pCreature20);
            cnt += 0x180;
        } else if (nVersion == 21) {
            pGlobalEntry = reinterpret_cast<CSavedGamePartyCreature*>(pGame + pHeader->m_partyCreatureTableOffset);
            sub_5A7BF0(reinterpret_cast<BYTE*>(pGlobalEntry), pCreature21);
            pCreature22 = reinterpret_cast<CSavedGamePartyCreature*>(pCreature21);
            cnt += 0x220;
        } else {
            pCreature22 = reinterpret_cast<CSavedGamePartyCreature*>(pGame + pHeader->m_partyCreatureTableOffset);
            cnt += sizeof(CSavedGamePartyCreature);
        }

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 7187
        UTIL_ASSERT(cnt <= (DWORD)nGame);

        if (bProgressBarInPlace) {
            ProgressBarCallback(dwProgressPerCreature, FALSE);

            if (g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
                while ((pData = pMessage->PollSpecificMessageType(CBaldurMessage::MSG_TYPE_RESOURCE,
                            CBaldurMessage::MSG_SUBTYPE_RESOURCE_DEMAND,
                            nMsgFrom,
                            dwSize))
                    != NULL) {
                    pMessage->OnResourceDemanded(nMsgFrom, pData, dwSize);
                    delete pData;
                }

                g_pChitin->m_bDisplayStale = TRUE;
                SleepEx(60, FALSE);
            }
        }

        if (pGlobalEntry->m_creatureResRef[0] != '\0') {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 7227
            UTIL_ASSERT(pGlobalEntry->m_creatureSize == 0);

            // NOTE: Uninline.
            creatureData.SetResRef(CResRef(pGlobalEntry->m_creatureResRef), TRUE, TRUE);

            DWORD nCreatureDataSize = creatureData.GetDataSize();
            if (nCreatureDataSize == 0) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
                // __LINE__: 7232
                UTIL_ASSERT_MSG(nCreatureDataSize,
                    CString("Creature file not found ") + CString(reinterpret_cast<char*>(pGlobalEntry->m_creatureResRef), RESREF_SIZE).GetBuffer(RESREF_SIZE + 1));
            }

            pSprite = new CGameSprite(creatureData.GetData(),
                nCreatureDataSize,
                0,
                0,
                -1,
                0,
                0,
                0x7FFFFFFF,
                CPoint(-1, -1),
                0);

            pSprite->SetResRef(CResRef(pGlobalEntry->m_creatureResRef));

            creatureData.ReleaseData();
        } else {
            cnt += pGlobalEntry->m_creatureSize;

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 7242
            UTIL_ASSERT(cnt <= (DWORD)nGame);

            if (pGlobalEntry->m_creatureOffset == 0 || pGlobalEntry->m_creatureSize == 0) {
                continue;
            }

            pSprite = new CGameSprite(pGame + pGlobalEntry->m_creatureOffset,
                pGlobalEntry->m_creatureSize,
                0,
                0,
                -1,
                0,
                0,
                0x7FFFFFFF,
                CPoint(-1, -1),
                0);
        }

        if (pSprite == NULL) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 7253
            UTIL_ASSERT(FALSE);
            continue;
        }

        pSprite->Unmarshal(pCreature22, FALSE, FALSE);
        pSprite->SetResRef(CResRef(pCreature22->m_creatureResRef));

        m_lstGlobalCreatures.AddTail(reinterpret_cast<int*>(static_cast<INT_PTR>(pSprite->GetId())));

        if (pSprite->GetAIType().m_nEnemyAlly == CAIObjectType::EA_FAMILIAR) {
            BOOL bInParty = FALSE;
            if (pSprite->GetId() != CGameObjectArray::INVALID_INDEX) {
                for (SHORT nPortrait = 0; nPortrait < 6; nPortrait++) {
                    if (m_characterPortraits[nPortrait] == pSprite->GetId()) {
                        bInParty = nPortrait != -1;
                        break;
                    }
                }
            }

            if (!bInParty
                && m_allies.Find(reinterpret_cast<int*>(static_cast<INT_PTR>(pSprite->GetId()))) == NULL
                && m_familiars.Find(reinterpret_cast<int*>(static_cast<INT_PTR>(pSprite->GetId()))) == NULL) {
                m_familiars.AddTail(reinterpret_cast<int*>(static_cast<INT_PTR>(pSprite->GetId())));
            }
        }

        if (pHeader->m_versionNumber == 0) {
            if (pSprite->m_baseStats.m_xp >= 89000) {
                pSprite->m_baseStats.m_xp = 89000;
            }
        }

        if (pHeader->m_versionNumber == 1) {
            if (pSprite->m_baseStats.m_xp >= 161000) {
                pSprite->m_baseStats.m_xp = 161000;
            }
        }

        CVariable creatureName;
        creatureName.SetName(CString(pSprite->GetScriptName()));
        creatureName.m_intValue = pSprite->GetId();
        m_namedCreatures.AddKey(creatureName);
    }

    if (pHeader->m_masterArea[0] != '\0') {
        // NOTE: 0x5A9660 builds the CString from the resref's bytes up to the
        // first NUL, as a `const char*`.
        LoadArea(CString(reinterpret_cast<char*>(pHeader->m_masterArea)), 255, FALSE, bProgressBarInPlace);
    }

    DWORD nCreatureOffset20 = 0;
    DWORD nCreatureOffset21 = 0;
    DWORD nCreatureOffset22 = 0;
    DWORD nCharacter = 0;
    while (nCharacter < pHeader->m_partyCreatureTableCount) {
        if (nVersion == 20) {
            sub_5A79E0(pGame + pHeader->m_partyCreatureTableOffset + nCreatureOffset20, pCreature20);
            sub_5A7BF0(pCreature20, pCreature21);
            pCreature22 = reinterpret_cast<CSavedGamePartyCreature*>(pCreature21);
            cnt += 0x180;
        } else if (nVersion == 21) {
            sub_5A7BF0(pGame + pHeader->m_partyCreatureTableOffset + nCreatureOffset21, pCreature21);
            pCreature22 = reinterpret_cast<CSavedGamePartyCreature*>(pCreature21);
            cnt += 0x220;
        } else {
            pCreature22 = reinterpret_cast<CSavedGamePartyCreature*>(pGame + pHeader->m_partyCreatureTableOffset + nCreatureOffset22);
            cnt += sizeof(CSavedGamePartyCreature);
        }

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 7331
        UTIL_ASSERT(cnt <= (DWORD)nGame);

        if (bProgressBarInPlace) {
            ProgressBarCallback(dwProgressPerCreature, FALSE);

            if (g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
                while ((pData = pMessage->PollSpecificMessageType(CBaldurMessage::MSG_TYPE_RESOURCE,
                            CBaldurMessage::MSG_SUBTYPE_RESOURCE_DEMAND,
                            nMsgFrom,
                            dwSize))
                    != NULL) {
                    pMessage->OnResourceDemanded(nMsgFrom, pData, dwSize);
                    delete pData;
                }

                g_pChitin->m_bDisplayStale = TRUE;
                SleepEx(60, FALSE);
            }
        }

        m_bPlayerScriptStyle = TRUE;

        if (pCreature22->m_creatureResRef[0] != '\0') {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 7371
            UTIL_ASSERT(pCreature22->m_creatureSize == 0);

            // NOTE: Uninline.
            creatureData.SetResRef(CResRef(pCreature22->m_creatureResRef), TRUE, TRUE);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 7373
            UTIL_ASSERT_MSG(creatureData.GetDataSize(),
                CString("Creature file not found ") + CString(reinterpret_cast<char*>(pCreature22->m_creatureResRef), RESREF_SIZE).GetBuffer(RESREF_SIZE + 1));

            pSprite = new CGameSprite(creatureData.GetData(),
                creatureData.GetDataSize(),
                0,
                0,
                -1,
                0,
                0,
                0x7FFFFFFF,
                CPoint(-1, -1),
                0);

            pSprite->SetResRef(CResRef(pCreature22->m_creatureResRef));

            creatureData.ReleaseData();

            m_bPlayerScriptStyle = FALSE;
        } else {
            cnt += pCreature22->m_creatureSize;

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 7384
            UTIL_ASSERT(cnt <= (DWORD)nGame);

            pSprite = new CGameSprite(pGame + pCreature22->m_creatureOffset,
                pCreature22->m_creatureSize,
                0,
                0,
                -1,
                0,
                0,
                0x7FFFFFFF,
                CPoint(-1, -1),
                0);

            m_bPlayerScriptStyle = FALSE;
        }

        if (pSprite == NULL) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 7391
            UTIL_ASSERT(FALSE);

            nCharacter++;
            nCreatureOffset20 += 0x180;
            nCreatureOffset21 += 0x220;
            nCreatureOffset22 += sizeof(CSavedGamePartyCreature);
            continue;
        }

        pSprite->SetResRef(CResRef(pCreature22->m_creatureResRef));

        if (bProgressBarInPlace && nCreatureOffset20 >= 0x180) {
            g_pChitin->cProgressBar.AddActionTarget(0);
        }

        EnablePortrait(static_cast<BYTE>(nCharacter), TRUE);

        pSprite->Unmarshal(pCreature22, TRUE, bProgressBarInPlace);

        nCreatureOffset20 += 0x180;
        nCreatureOffset21 += 0x220;
        nCreatureOffset22 += sizeof(CSavedGamePartyCreature);

        if (nCharacter++ == pHeader->m_nPCAreaViewed && pSprite->GetArea() != NULL) {
            m_visibleArea = pSprite->GetArea()->m_id;
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(pSprite->GetId(),
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);

        if (pHeader->m_versionNumber == 0) {
            if (pSprite->m_baseStats.m_xp >= 89000) {
                pSprite->m_baseStats.m_xp = 89000;
            }
        }

        if (pHeader->m_versionNumber == 1) {
            if (pSprite->m_baseStats.m_xp >= 161000) {
                pSprite->m_baseStats.m_xp = 161000;
            }
        }

        CVariable creatureName;
        creatureName.SetName(CString(pSprite->GetScriptName()));
        creatureName.m_intValue = pSprite->GetId();
        m_namedCreatures.AddKey(creatureName);

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(pSprite->GetId(),
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    SelectToolbar();
    SetProtagonist(m_characters[0]);

    if (pHeader->m_versionNumber != 3) {
        ReputationAdjustment(100, TRUE);
    }

    if (g_pBaldurChitin->cNetwork.GetSessionHosting() == TRUE && g_pBaldurChitin->field_4A24) {
        CScreenWorld* pWorldScreen = g_pBaldurChitin->m_pEngineWorld;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 7462
        UTIL_ASSERT(pWorldScreen != NULL);

        CRect rViewSize = pWorldScreen->GetNewViewSize();
        CUIPanel* pLeftPanel = pWorldScreen->GetManager()->GetPanel(1);
        CUIPanel* pBottomPanel = pWorldScreen->GetManager()->GetPanel(pWorldScreen->GetPanel_22_0());

        if ((pHeader->m_dwFlags & 0x20) != 0) {
            pWorldScreen->field_11F0 = 0;
            pWorldScreen->field_11F8 = rViewSize.bottom;
            pLeftPanel->m_bActive = FALSE;
            rViewSize.bottom = CInfinity::stru_8E79E0.bottom;
            CInfinity::stru_8E79B8.bottom = rViewSize.bottom;
            pWorldScreen->SetNewViewSize(rViewSize, FALSE);
            pWorldScreen->GetManager()->InvalidateRect(NULL);
        } else if ((pHeader->m_dwFlags & 0x40) != 0) {
            pWorldScreen->field_11FC = rViewSize.bottom;
            pWorldScreen->field_11F4 = 0;
            pBottomPanel->m_bActive = FALSE;
            pLeftPanel->m_ptOrigin.x = pLeftPanel->m_ptBaseOrigin.x;
            pLeftPanel->m_ptOrigin.y = pLeftPanel->m_ptBaseOrigin.y + pBottomPanel->m_size.cy;
            pLeftPanel->m_bActive = TRUE;
            rViewSize.bottom = CInfinity::stru_8E79C8.bottom;
            CInfinity::stru_8E79B8.bottom = rViewSize.bottom;
            pWorldScreen->SetNewViewSize(rViewSize, FALSE);
            pWorldScreen->GetManager()->InvalidateRect(NULL);
        }

        if ((pHeader->m_dwFlags & 0x10) != 0) {
            g_pBaldurChitin->m_pEngineWorld->GetManager()->m_bHidden = TRUE;
            g_pBaldurChitin->m_pEngineWorld->field_11D0 = rViewSize;
            g_pBaldurChitin->m_pEngineWorld->SetNewViewSize(CInfinity::stru_8E7548, FALSE);
        }
    }

    cnt += sizeof(CCreatureFileItem) * pHeader->m_partyInventoryTableCount;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 7503
    UTIL_ASSERT(cnt <= (DWORD)nGame);

    if (bProgressBarInPlace) {
        ProgressBarCallback(dwProgressFinal, FALSE);
    }

    field_1B84 = 1;

    CVariable variable;
    for (DWORD nVariable = 0; nVariable < pHeader->m_globalVariablesCount; nVariable++) {
        if ((nVariable & 0xF) == 0) {
            g_pChitin->m_bDisplayStale = TRUE;
            SleepEx(60, FALSE);
        }

        // NOTE: Uninline.
        variable = *reinterpret_cast<CVariable*>(pGame + pHeader->m_globalVariablesOffset + sizeof(CVariable) * nVariable);

        cnt += sizeof(CVariable);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 7525
        UTIL_ASSERT(cnt <= (DWORD)nGame);

        m_variables.AddKey(variable);
    }

    field_1B84 = 0;

    if (bProgressBarInPlace) {
        ProgressBarCallback(dwProgressFinal, FALSE);
    }

    m_cJournal.ClearAllEntries();

    if (pHeader->m_journalEntriesCount != 0) {
        m_cJournal.Unmarshal(reinterpret_cast<CSavedGameJournalEntry*>(pGame + pHeader->m_journalEntriesOffset),
            pHeader->m_journalEntriesCount);

        cnt += sizeof(CSavedGameJournalEntry) * pHeader->m_journalEntriesCount;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 7596
        UTIL_ASSERT(cnt <= (DWORD)nGame);
    }

    if (pHeader->m_familiarsOffset != 0) {
        DWORD* pFamiliars = reinterpret_cast<DWORD*>(pGame + pHeader->m_familiarsOffset);
        memcpy(m_defaultFamiliarResRefs, pFamiliars, sizeof(m_defaultFamiliarResRefs));

        DWORD nFamiliars = 0;
        DWORD* pFamiliarCounts = pFamiliars + 19;
        for (INT nAlignment = 0; nAlignment < 9; nAlignment++) {
            for (INT nLevel = 0; nLevel < 9; nLevel++) {
                for (DWORD nCount = *pFamiliarCounts; nCount > 0; nCount--) {
                    CResRef* pResRef = new CResRef(pGame + pFamiliars[18] + RESREF_SIZE * nFamiliars);
                    m_familiarResRefs[nAlignment][nLevel].AddTail(pResRef);
                    nFamiliars++;
                }
                pFamiliarCounts++;
            }
        }

        cnt += 400 + RESREF_SIZE * nFamiliars;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 7615
        UTIL_ASSERT(cnt <= (DWORD)nGame);
    }

    if (sFileVersion.CompareNoCase("V1.1") != 0) {
        POSITION pos = field_4BDC.GetHeadPosition();
        while (pos != NULL) {
            POSITION posOld = pos;
            CInfGameAreaLevel* pAreaLevel = field_4BDC.GetNext(pos);
            if (pAreaLevel != NULL) {
                delete pAreaLevel;
            }
            field_4BDC.RemoveAt(posOld);
        }

        BYTE* pAreaLevels = pGame + cnt;
        LONG nAreaLevels = *reinterpret_cast<LONG*>(pAreaLevels);
        pAreaLevels += 4;

        for (; nAreaLevels > 0; nAreaLevels--) {
            CInfGameAreaLevel* pAreaLevel = new CInfGameAreaLevel();
            if (pAreaLevel != NULL) {
                pAreaLevel->m_dwFlags = 0;
                pAreaLevel->m_nLevel = 0;
                memset(pAreaLevel->field_9, 0, sizeof(pAreaLevel->field_9));

                DWORD nSize = *reinterpret_cast<DWORD*>(pAreaLevels);
                DWORD nLength = *reinterpret_cast<DWORD*>(pAreaLevels + 4);
                pAreaLevel->m_dwFlags = *reinterpret_cast<DWORD*>(pAreaLevels + 8);
                pAreaLevel->m_nLevel = static_cast<BYTE>(*reinterpret_cast<DWORD*>(pAreaLevels + 12));
                memcpy(pAreaLevel->field_9, pAreaLevels + 13, sizeof(pAreaLevel->field_9));

                memcpy(pAreaLevel->m_sArea.GetBuffer(nLength), pAreaLevels + 28, nLength);
                pAreaLevel->m_sArea.ReleaseBuffer(nLength);

                field_4BDC.AddTail(pAreaLevel);

                pAreaLevels += nSize + 4;
            }
        }

        DWORD nAreaLevelsSize = 4;
        pos = field_4BDC.GetHeadPosition();
        while (pos != NULL) {
            CInfGameAreaLevel* pAreaLevel = field_4BDC.GetNext(pos);
            nAreaLevelsSize += pAreaLevel->m_sArea.GetLength() + 28;
        }

        cnt += nAreaLevelsSize;
    }

    if (bProgressBarInPlace) {
        ProgressBarCallback(dwProgressFinal, FALSE);
    }

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 7634
    UTIL_ASSERT(cnt == (DWORD)nGame);
}

// 0x5A1A80
CString CInfGame::GetDirSounds()
{
    return m_sSoundsDir;
}

// 0x5A1AB0
void CInfGame::CharacterExport(LONG nCharacterId, CString sFileName)
{
    CResHelper<CResCHR, 1018> cCharacterFile;
    CResHelper<CResText, 1022> cBiographyFile;
    BOOLEAN bTemporaryKey = FALSE;
    CGameSprite* pSprite;
    BYTE* pCharacter;
    LONG nCharacter;
    WORD facing;

    if (m_cObjectArray.GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE)
        != CGameObjectArray::SUCCESS) {
        return;
    }

    pCharacter = NULL;
    pSprite->Marshal(&pCharacter, &nCharacter, &facing, FALSE, FALSE);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 4302
    UTIL_ASSERT(pCharacter != NULL);

    LONG nExportCharacter = nCharacter + 0x224;
    BYTE* pExportCharacter = new BYTE[nExportCharacter];

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 4307
    UTIL_ASSERT(pExportCharacter != NULL);

    memset(pExportCharacter, 0, nExportCharacter);
    memcpy(pExportCharacter, "CHR V2.2", 8);

    BYTE* pHeader = pExportCharacter + 8;
    memset(pHeader, 0, SCRIPTNAME_SIZE);
    strncpy(reinterpret_cast<char*>(pHeader), pSprite->GetName(), SCRIPTNAME_SIZE);

    for (INT nWeapon = 0; nWeapon < 8; nWeapon++) {
        *reinterpret_cast<SHORT*>(pHeader + 0x28 + 2 * nWeapon) = pSprite->m_quickWeapons[nWeapon].m_abilityId.m_itemNum;
        *reinterpret_cast<SHORT*>(pHeader + 0x38 + 2 * nWeapon) = pSprite->m_quickWeapons[nWeapon].m_abilityId.m_abilityNum;
        pHeader[0x19C + nWeapon] = pSprite->field_3D3A[nWeapon];
    }

    for (INT nSpell = 0; nSpell < 9; nSpell++) {
        pSprite->m_quickSpells[nSpell].m_abilityId.m_res.GetResRef(pHeader + 0x48 + RESREF_SIZE * nSpell);
        pHeader[0x90 + nSpell] = pSprite->m_quickSpells[nSpell].m_abilityId.m_nClass;
        pHeader[0x15C + nSpell] = pSprite->m_quickSpells[nSpell].m_abilityId.field_1D;
        pHeader[0x165 + nSpell] = static_cast<BYTE>(pSprite->m_quickSpells[nSpell].m_abilityId.field_1E);
    }

    for (INT nItem = 0; nItem < 3; nItem++) {
        *reinterpret_cast<SHORT*>(pHeader + 0x9A + 2 * nItem) = pSprite->m_quickItems[nItem].m_abilityId.m_itemNum;
        *reinterpret_cast<SHORT*>(pHeader + 0xA0 + 2 * nItem) = pSprite->m_quickItems[nItem].m_abilityId.m_abilityNum;
    }

    for (INT nInnate = 0; nInnate < 9; nInnate++) {
        pSprite->m_quickInnates[nInnate].m_abilityId.m_res.GetResRef(pHeader + 0xA6 + RESREF_SIZE * nInnate);
    }

    for (INT nSong = 0; nSong < 9; nSong++) {
        pSprite->m_quickSongs[nSong].m_abilityId.m_res.GetResRef(pHeader + 0xEE + RESREF_SIZE * nSong);
    }

    for (INT nButton = 0; nButton < 9; nButton++) {
        *reinterpret_cast<int*>(pHeader + 0x136 + 4 * nButton) = pSprite->field_3D14[nButton];
    }

    pHeader[0x15A] = pSprite->m_nLastSpellbookClassIndex;
    pHeader[0x15B] = pSprite->m_nLastSpellbookSpellLevel;
    pHeader[0x170] = pSprite->m_nLastSong;

    *reinterpret_cast<DWORD*>(pHeader + 0x20) = 0x224;
    *reinterpret_cast<DWORD*>(pHeader + 0x24) = nCharacter;

    CResRef cResSounds;
    cResSounds = pSprite->m_secondarySounds;
    cResSounds.GetResRef(pHeader + 0x174);

    CString sSounds;
    sSounds = reinterpret_cast<char*>(pSprite->field_725A);
    memcpy(pHeader + 0x17C, static_cast<LPCSTR>(sSounds), sSounds.GetLength());

    memcpy(pExportCharacter + 0x224, pCharacter, nCharacter);
    delete[] pCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 4364
    UTIL_ASSERT(nCharacter + 0x224 == nExportCharacter);

    // NOTE: Uninline.
    cCharacterFile.SetResRef(CResRef(sFileName), TRUE, TRUE);
    if (cCharacterFile.GetResRef() == "") {
        bTemporaryKey = TRUE;
        g_pChitin->cDimm.AddTemporaryKey(m_sCharactersDir, CResRef(sFileName), 1018);

        // NOTE: Uninline.
        cCharacterFile.SetResRef(CResRef(sFileName), TRUE, TRUE);
    }

    static_cast<CRes*>(cCharacterFile.GetRes())->Write(m_sCharactersDir, pExportCharacter, nExportCharacter);

    if (bTemporaryKey == TRUE) {
        g_pChitin->cDimm.RemoveTemporaryKey(m_sCharactersDir, CResRef(sFileName), 1018);
    }

    delete[] pExportCharacter;

    STR_RES strRes;
    g_pBaldurChitin->GetTlkTable().Fetch(pSprite->m_baseStats.m_biography, strRes);

    // NOTE: Uninline.
    cBiographyFile.SetResRef(CResRef(sFileName), TRUE, TRUE);

    bTemporaryKey = FALSE;
    if (cBiographyFile.GetResRef() == "") {
        bTemporaryKey = TRUE;
        g_pChitin->cDimm.AddTemporaryKey(m_sCharactersDir, CResRef(sFileName), 1022);

        // NOTE: Uninline.
        cBiographyFile.SetResRef(CResRef(sFileName), TRUE, TRUE);
    }

    INT nLength = strRes.szText.GetLength();
    static_cast<CRes*>(cBiographyFile.GetRes())->Write(m_sCharactersDir, strRes.szText.GetBuffer(0), nLength + (nLength > 0));

    if (bTemporaryKey == TRUE) {
        g_pChitin->cDimm.RemoveTemporaryKey(m_sCharactersDir, CResRef(sFileName), 1022);
    }

    m_cObjectArray.ReleaseShare(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// 0x5A24D0
CGameArea* CInfGame::LoadArea(CString areaName, BYTE nTravelScreenImageToUse, BOOLEAN bProgressBarRequired, BOOLEAN bProgressBarInPlace)
{
    CAreaFile areaData;
    CString sLevel;
    CString sResRef;
    LONG highRequests;
    LONG mediumRequests;
    LONG lowRequests;
    DWORD nTotalCachingRequirements;
    BYTE cnt;
    CGameArea* pArea;

    areaName.MakeUpper();

    pArea = GetArea(areaName);
    if (pArea != NULL) {
        if (bProgressBarInPlace) {
            ProgressBarCallback(3000000, FALSE);
        }
        return pArea;
    }

    if (!bProgressBarInPlace) {
        g_pChitin->m_bDisplayStale = TRUE;
        SleepEx(25, FALSE);
    }

    BYTE nAverageLevel = 0;
    for (SHORT nPortrait = 0; nPortrait < m_nCharacters; nPortrait++) {
        CGameSprite* pSprite;
        LONG nCharacterId = m_characterPortraits[nPortrait];
        BYTE rc = m_cObjectArray.GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            nAverageLevel += pSprite->GetDerivedStats()->m_nLevel;
            m_cObjectArray.ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    if (m_nCharacters > 0) {
        sLevel.Format("%d", nAverageLevel / m_nCharacters);
    }

    highRequests = g_pChitin->cDimm.RequestsPendingCount(CDimm::PRIORITY_HIGH);
    mediumRequests = g_pChitin->cDimm.RequestsPendingCount(CDimm::PRIORITY_MEDIUM);
    lowRequests = g_pChitin->cDimm.RequestsPendingCount(CDimm::PRIORITY_LOW);

    cnt = 0;
    while (cnt < CINFGAME_MAX_AREAS) {
        if (m_gameAreas[cnt] == NULL) {
            break;
        }
        cnt++;
    }

    if (cnt >= CINFGAME_MAX_AREAS) {
        return NULL;
    }

    m_gameAreas[cnt] = new CGameArea(cnt);
    if (m_gameAreas[cnt] == NULL) {
        return NULL;
    }

    // The original tests the byte as a signed char against -1; callers pass
    // 255 (or -1) for "pick one", which rolls rand() here.
    if (nTravelScreenImageToUse != 255) {
        m_nTravelScreenImageToUse = nTravelScreenImageToUse;
    } else {
        m_nTravelScreenImageToUse = rand() % 5;
    }

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
        if (g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->GetListenToJoinOption() == TRUE
            && g_pBaldurChitin->GetActiveEngine() != g_pBaldurChitin->m_pEngineLoad) {
            g_pBaldurChitin->GetBaldurMessage()->ResourceSuggestLoad(areaName,
                1010,
                m_nTravelScreenImageToUse | (bProgressBarInPlace | bProgressBarRequired) << 8);
        }
        SleepEx(200, FALSE);
    }

    g_pChitin->m_bDisplayStale = TRUE;
    sub_59FA00(TRUE);

    nTotalCachingRequirements = CachingRequirements(areaName);
    if (bProgressBarRequired == TRUE || nTotalCachingRequirements > 0) {
        bProgressBarRequired = TRUE;
        nTotalCachingRequirements += 5000000;
    } else {
        bProgressBarRequired = FALSE;
    }

    nTotalCachingRequirements += 3000000;

    if (bProgressBarInPlace == TRUE) {
        if (bProgressBarRequired == TRUE) {
            g_pChitin->cProgressBar.AddActionTarget(nTotalCachingRequirements - 3000000);
        } else {
            ProgressBarCallback(3000000 - nTotalCachingRequirements, FALSE);
        }
    }

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && !g_pChitin->cNetwork.GetSessionHosting()
        && !g_pBaldurChitin->GetBaldurMessage()->DemandResourceFromServer(areaName, 1010, TRUE, FALSE, TRUE)) {
        g_pChitin->cNetwork.CloseSession(TRUE);
        m_bInDestroyGame = TRUE;
        delete m_gameAreas[cnt];
        m_gameAreas[cnt] = NULL;
        m_bInDestroyGame = FALSE;
        return NULL;
    }

    if (bProgressBarRequired == TRUE
        && !bProgressBarInPlace) {
        field_1B86 = areaName;
        g_pChitin->SetProgressBar(TRUE,
            9886,
            0,
            0,
            TRUE,
            9885,
            FALSE,
            0,
            FALSE,
            FALSE,
            255);
        g_pChitin->cProgressBar.m_nActionProgress = 0;
        g_pChitin->cProgressBar.m_nActionTarget = nTotalCachingRequirements;
    }

    CacheResFileWithResource(areaName);

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
        if (!g_pChitin->cNetwork.GetSessionHosting()) {
            if (bProgressBarInPlace || bProgressBarRequired) {
                if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
                    g_pChitin->cProgressBar.m_nWaitingReason = 16513;
                    g_pChitin->cProgressBar.m_bWaiting = TRUE;
                }

                if (!g_pBaldurChitin->GetBaldurMessage()->DemandResourceFromServer(areaName, 1010, FALSE, TRUE, TRUE)) {
                    g_pChitin->cNetwork.CloseSession(TRUE);
                    g_pChitin->SetProgressBar(FALSE,
                        0,
                        0,
                        0,
                        FALSE,
                        0,
                        FALSE,
                        0,
                        FALSE,
                        FALSE,
                        255);
                    m_bInDestroyGame = TRUE;
                    delete m_gameAreas[cnt];
                    m_gameAreas[cnt] = NULL;
                    m_bInDestroyGame = FALSE;
                }

                if (bProgressBarInPlace || bProgressBarRequired) {
                    g_pChitin->cProgressBar.m_bWaiting = FALSE;
                    g_pBaldurChitin->m_cCachingStatus.InvalidateScreen();
                }
            }
        }

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
            m_gameAreas[cnt]->m_nInitialAreaID = 0;
            g_pBaldurChitin->GetObjectGame()->m_nAreaFirstObject = -1;

            // TODO: Unclear.
            DWORD idPlayer = -1;
            BOOLEAN bIDInUse = TRUE;
            while (bIDInUse) {
                bIDInUse = g_pChitin->cNetwork.FindPlayerLocationByID(idPlayer, FALSE) != -1;
                for (INT nArea = 0; nArea < CINFGAME_MAX_AREAS; nArea++) {
                    if (m_gameAreas[nArea] != NULL
                        && m_gameAreas[nArea]->m_nInitialAreaID == idPlayer) {
                        bIDInUse = TRUE;
                        break;
                    }
                }
                if (bIDInUse) {
                    idPlayer--;
                }
            }

            m_gameAreas[cnt]->m_nInitialAreaID = idPlayer;
            m_nUniqueAreaID = idPlayer;
        }
    }

    sub_59FA00(TRUE);

    m_bInLoadArea = TRUE;

    areaData.SetResRef(CResRef(areaName), TRUE, TRUE);
    m_gameAreas[cnt]->m_resRef = areaName;
    m_gameAreas[cnt]->m_cGameAreaNotes.m_rArea = CResRef(areaName);
    m_gameAreas[cnt]->Unmarshal(areaData.GetData(),
        areaData.GetDataSize(),
        areaName,
        bProgressBarInPlace);

    areaData.ReleaseData();

    if (g_pBaldurChitin->cNetwork.GetSessionHosting() == TRUE
        && !g_pBaldurChitin->GetObjectGame()->m_bInLoadGame) {
        m_gameAreas[cnt]->SortLists();
        m_gameAreas[cnt]->EnterSpawn();
    }

    POSITION pos = m_lstGlobalCreatures.GetHeadPosition();
    while (pos != NULL) {
        CGameSprite* pSprite;
        LONG iSprite = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lstGlobalCreatures.GetNext(pos)));
        BYTE rc = m_cObjectArray.GetShare(iSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            if (pSprite->m_currentArea == areaName) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
                // __LINE__: 4774
                UTIL_ASSERT(pSprite->GetArea() == NULL);

                rc = m_cObjectArray.GetDeny(iSprite,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
                if (rc == CGameObjectArray::SUCCESS) {
                    if ((pSprite->GetBaseStats()->m_generalState & STATE_DEAD) != 0
                        && pSprite->GetAnimation()->CanLieDown()) {
                        pSprite->AddToArea(m_gameAreas[cnt],
                            pSprite->GetPos(),
                            0,
                            CGameObject::LIST_BACK);
                    } else {
                        pSprite->AddToArea(m_gameAreas[cnt],
                            pSprite->GetPos(),
                            0,
                            CGameObject::LIST_FRONT);
                    }
                    m_cObjectArray.ReleaseDeny(iSprite,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                }
            }
            m_cObjectArray.ReleaseShare(iSprite,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    m_bInLoadArea = FALSE;

    m_gameAreas[cnt]->m_nFirstObject = g_pBaldurChitin->GetObjectGame()->m_nAreaFirstObject;

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
        g_pBaldurChitin->GetObjectGame()->GetRemoteObjectArray()->ChangeControlOnLoadGame();
    }

    if (bProgressBarRequired) {
        if (g_pChitin->field_4C == 1 && !g_pChitin->m_bExitRSThread) {
            int v1 = highRequests + mediumRequests + lowRequests;
            int v6 = 0;
            g_pChitin->cDimm.MoveRequests(CDimm::PRIORITY_HIGH, CDimm::PRIORITY_LOW, highRequests);
            g_pChitin->cDimm.MoveRequests(CDimm::PRIORITY_MEDIUM, CDimm::PRIORITY_LOW, mediumRequests);
            g_pChitin->cDimm.MoveRequests(CDimm::PRIORITY_LOW, CDimm::PRIORITY_LOW, lowRequests);
            int v2 = g_pChitin->cDimm.RequestsPendingCount() - v1;
            int v3 = v2;
            while (v3 > 0) {
                if (v6 >= 100) {
                    break;
                }

                if (!g_pChitin->cDimm.ResumeServicing()
                    || g_pChitin->cDimm.MemoryAlmostFull()) {
                    int v4 = v1 - g_pChitin->cDimm.RequestsPendingCount();
                    g_pChitin->cDimm.ReduceServicedList();
                    v1 = g_pChitin->cDimm.RequestsPendingCount() + v4;
                    g_pChitin->cDimm.ResumeServicing();
                }

                g_pChitin->cDimm.field_0 = 0;
                g_pChitin->cDimm.field_4 = 0;
                SleepEx(50, FALSE);

                int v5 = g_pChitin->cDimm.RequestsPendingCount() - v1;
                if (v5 != v3) {
                    g_pChitin->cProgressBar.m_nActionProgress += (v3 - v5) * (5000000 / v2);
                    v3 = v5;
                    g_pChitin->m_bDisplayStale = TRUE;
                }

                v6++;
            }
        }

        if (!bProgressBarInPlace && bProgressBarRequired == TRUE) {
            g_pChitin->cProgressBar.m_nActionProgress = g_pChitin->cProgressBar.m_nActionTarget;
            g_pChitin->cProgressBar.m_bDisableMinibars = TRUE;
            g_pChitin->m_bDisplayStale = TRUE;
            sub_59FA00(TRUE);
            g_pChitin->SetProgressBar(FALSE,
                0,
                0,
                0,
                FALSE,
                0,
                FALSE,
                0,
                FALSE,
                FALSE,
                255);
        }
    }

    m_nTravelScreenImageToUse = rand() % 5;

    return m_gameAreas[cnt];
}

// 0x5AC110
BOOLEAN CInfGame::CanSaveGame(STRREF& strError, BOOLEAN bInRestGame, BOOLEAN bInStore)
{
    if (!bInRestGame
        && g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && !g_pChitin->cNetwork.GetSessionHosting()) {
        // "Client machines are not allowed to save in a multiplayer game."
        strError = 17194;
        return FALSE;
    }

    if (g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode == 386
        || g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode == 1282) {
        // "Cannot save game while in dialog mode."
        strError = 19253;
        return FALSE;
    }

    if (g_pBaldurChitin->GetScreenWorld()->m_comingOutOfDialog > 0 && !bInStore) {
        // "Cannot save game while in dialog mode."
        strError = 19253;
        return FALSE;
    }

    if (g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->m_bHostPermittedDialog == TRUE) {
        // "Cannot save because dialog is about to start."
        strError = 19254;
        return FALSE;
    }

    if (g_pBaldurChitin->GetScreenWorld()->m_bHardPaused == TRUE) {
        // "Cannot save during a rest, chapter information or movie."
        strError = 19255;
        return FALSE;
    }

    if (g_pBaldurChitin->GetActiveEngine() == g_pBaldurChitin->m_pEngineProjector
        || g_pBaldurChitin->GetActiveEngine() == g_pBaldurChitin->m_pEngineChapter
        || g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_cutScene) {
        // "Cannot save during a rest, chapter information or movie."
        strError = 19255;
        return FALSE;
    }

    if (!bInRestGame
        && g_pBaldurChitin->GetActiveEngine() == g_pBaldurChitin->m_pEngineStore) {
        // "Cannot save while someone is in the Store Screen."
        strError = 10841;
        return FALSE;
    }

    if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
        && !bInRestGame
        && sub_5C93E0() > 0) {
        // "Cannot save while someone is in the Store Screen."
        strError = 10841;
        return FALSE;
    }

    if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
        && !bInRestGame
        && g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->sub_5186A0() > 0) {
        // "Cannot save while someone is levelling up."
        strError = 41397;
        return FALSE;
    }

    if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
        && !bInRestGame
        && g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->sub_518560() > 0) {
        // "Cannot save while someone is in the Inventory Screen."
        strError = 26546;
        return FALSE;
    }

    for (BYTE cnt = 0; cnt < CINFGAME_MAX_AREAS; cnt++) {
        if (m_gameAreas[cnt] != NULL) {
            if (!m_gameAreas[cnt]->CanSaveGame(strError)) {
                return FALSE;
            }
        }
    }

    strError = -1;
    return TRUE;
}

// 0x5AC340
BOOL CInfGame::BackupQuickSave()
{
    BOOL bResult = FALSE;

    m_sSaveGame = QUICK_SAVE_BACKUP_NAME;
    CScreenCharacter::SAVE_NAME = QUICK_SAVE_BACKUP_NAME;

    CString sDirSave = GetDirSave();
    if (g_pChitin->cDimm.DirectoryRemoveFiles(sDirSave)) {
        m_sSaveGame = QUICK_SAVE_NAME;
        CScreenCharacter::SAVE_NAME = QUICK_SAVE_NAME;
        bResult = g_pChitin->cDimm.DirectoryCopyFiles(GetDirSave(), sDirSave);
    }

    return bResult;
}

// 0x5A9680
void CInfGame::ProgressBarCallback(DWORD dwSize, BOOLEAN bInitialize)
{
    if (bInitialize == TRUE) {
        m_dwLastProgressRenderTickCount = GetTickCount() - 250;
        m_dwLastProgressMsgTickCount = GetTickCount() - 5000;
    }

    g_pChitin->cProgressBar.m_nActionProgress += dwSize;

    if (GetTickCount() - m_dwLastProgressMsgTickCount >= 1000) {
        m_dwLastProgressMsgTickCount = GetTickCount();

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
            g_pBaldurChitin->GetBaldurMessage()->SendProgressBarStatus(g_pChitin->cProgressBar.m_nActionProgress,
                g_pChitin->cProgressBar.m_nActionTarget,
                g_pChitin->cProgressBar.m_bWaiting,
                g_pChitin->cProgressBar.m_nWaitingReason,
                g_pChitin->cProgressBar.m_bTimeoutVisible,
                g_pChitin->cProgressBar.m_nSecondsToTimeout);
        }
    }

    if (GetTickCount() - m_dwLastProgressRenderTickCount >= 250) {
        m_dwLastProgressRenderTickCount = GetTickCount();

        g_pChitin->m_bDisplayStale = TRUE;
        g_pChitin->cDimm.field_0 = 1;
        g_pChitin->cDimm.field_4 = 1;
        SleepEx(25, TRUE);

        sub_59FA00(FALSE);
    }
}

// 0x5A9780
char CInfGame::sub_5A9780(BYTE nKey)
{
    if (nKey >= 'A' && nKey <= 'Z') {
        return static_cast<char>(nKey);
    }

    if (nKey >= '0' && nKey <= '9') {
        return static_cast<char>(nKey);
    }

    for (int sym = 0; sym < CINFGAME_SPECIAL_SYMBOLS; sym++) {
        if (byte_8ABA3C[sym] == nKey) {
            return byte_8ABA24[sym];
        }
    }

    return -1;
}

// 0x5A97D0
BYTE CInfGame::sub_5A97D0(char ch)
{
    if (ch >= 'a' && ch <= 'z') {
        return static_cast<BYTE>(ch - 32);
    }

    if (ch >= 'A' && ch <= 'Z') {
        return static_cast<BYTE>(ch);
    }

    if (ch >= '0' && ch <= '9') {
        return static_cast<BYTE>(ch);
    }

    for (int sym = 0; sym < CINFGAME_SPECIAL_SYMBOLS; sym++) {
        if (byte_8ABA24[sym] == ch) {
            return byte_8ABA3C[sym];
        }
    }

    return -1;
}

// 0x5A9830
BYTE CInfGame::sub_5A9830(int index)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 7779
    UTIL_ASSERT(index >= 0 && index < CINFGAME_KEYMAP_SIZE);

    if (index >= FIFTY_THREE) {
        return 0;
    }

    // NOTE: Uninline.
    return sub_5A97D0(off_8AFD24[index][0]);
}

// 0x5A98F0
void CInfGame::LoadKeymap()
{
    CString sKey;

    memset(m_pKeymap, 0, sizeof(m_pKeymap));
    memset(m_pKeymapFlags, 0, sizeof(m_pKeymapFlags));

    // NOTE: Unsigned compare in both loops.
    WORD index;
    for (index = 0; index < FIFTY_THREE; index++) {
        char* buffer = sKey.GetBuffer(128);
        GetPrivateProfileStringA("Keymap",
            off_8AFC50[index],
            off_8AFD24[index],
            buffer,
            128,
            ".\\Keymap.ini");
        sKey.ReleaseBuffer();
        LoadKey(sKey, index);
    }

    for (; index < CINFGAME_KEYMAP_SIZE; index++) {
        char* buffer = sKey.GetBuffer(128);
        if (index >= 199) {
            GetPrivateProfileStringA("Wizard Spells",
                off_8B4F84[index - FIFTY_THREE],
                "",
                buffer,
                128,
                ".\\Keymap.ini");
        } else {
            GetPrivateProfileStringA("Priest Spells",
                off_8B4F84[index - FIFTY_THREE],
                "",
                buffer,
                128,
                ".\\Keymap.ini");
        }
        sKey.ReleaseBuffer();
        LoadKey(sKey, index);
    }
}

// 0x5A9A30
void CInfGame::LoadKey(const CString& sKey, SHORT index)
{
    INT nOffset = 0;
    if (sKey.GetLength() > 0) {
        if (sKey.Find("CTRL-") == 0
            || sKey.Find("ctrl-") == 0
            || sKey.Find("Ctrl-") == 0) {
            nOffset = strlen("CTRL-");
            m_pKeymapFlags[index] = 1;
        }

        if (sKey[nOffset] >= 'a' && sKey[nOffset] <= 'z') {
            m_pKeymap[index] = sKey[nOffset] - 32;
        } else {
            m_pKeymap[index] = sKey[nOffset];

            for (int sym = 0; sym < CINFGAME_SPECIAL_SYMBOLS; sym++) {
                if (byte_8ABA24[sym] == sKey[nOffset]) {
                    m_pKeymap[index] = byte_8ABA3C[sym];
                    break;
                }
            }
        }
    }
}

// 0x5A9B00
void CInfGame::SetKey(INT nIndex, BYTE nKey, BYTE nKeyFlag)
{
    if (nIndex >= 0 && nIndex < CINFGAME_KEYMAP_SIZE) {
        m_pKeymap[nIndex] = nKey;
        m_pKeymapFlags[nIndex] = nKeyFlag;

        for (int sym = 0; sym < CINFGAME_SPECIAL_SYMBOLS; sym++) {
            if (byte_8ABA3C[sym] == nKey) {
                nKey = byte_8ABA24[sym];
                break;
            }
        }

        if (nIndex < FIFTY_THREE) {
            char buffer[16];
            sprintf(buffer, "%s%c", nKeyFlag ? "CTRL-" : "", nKey);
            WritePrivateProfileStringA("Keymap",
                off_8AFC50[nIndex],
                buffer,
                ".\\Keymap.ini");
        }
    }
}

// 0x5A9BA0
void CInfGame::LoadOptions()
{
    m_cOptions.m_nTilesPrecachePercent = GetPrivateProfileIntA("Game Options",
        "Tiles Precache Percent",
        100,
        g_pBaldurChitin->GetIniFileName());

    if (m_cOptions.m_nTilesPrecachePercent > 100) {
        m_cOptions.m_nTilesPrecachePercent = 100;
    }

    m_cOptions.m_bDuplicateFloatingText = GetPrivateProfileIntA("Game Options",
        "Duplicate Floating Text",
        1,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.m_bTranslucentBlts = GetPrivateProfileIntA("Game Options",
        "Translucent Blts",
        1,
        g_pBaldurChitin->GetIniFileName());
    CInfinity::TRANSLUCENT_BLTS_ON = m_cOptions.m_bTranslucentBlts;
    CVidCell::TRANSLUCENT_BLTS_ON = m_cOptions.m_bTranslucentBlts;

    m_cOptions.m_bMaxHP = GetPrivateProfileIntA("Game Options",
        "Maximum HP",
        0,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.m_bAutoPauseOnTrap = GetPrivateProfileIntA("Game Options",
        "Pause On Trap",
        0,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.field_AC = 0;

    m_cOptions.m_bEnvironmentalAudio = GetPrivateProfileIntA("Game Options",
        "Environmental Audio",
        1,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.m_bCheatsEnabled = GetPrivateProfileIntA("Game Options",
        "Cheats",
        0,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.m_nGoreOption = GetPrivateProfileIntA("Game Options",
        "Grant 2000",
        0,
        g_pBaldurChitin->GetIniFileName());

    if (m_cOptions.m_nGoreOption) {
        m_cOptions.m_nGore = GetPrivateProfileIntA("Game Options",
            "Chucknomitron",
            0,
            g_pBaldurChitin->GetIniFileName());
    } else {
        m_cOptions.m_nGore = 0;
    }

    m_cOptions.m_bFootStepsSounds = GetPrivateProfileIntA("Game Options",
        "Footsteps",
        1,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.m_nMouseScrollSpeed = GetPrivateProfileIntA("Game Options",
        "Mouse Scroll Speed",
        28,
        g_pBaldurChitin->GetIniFileName());

    if (m_cOptions.m_nMouseScrollSpeed < 6 || m_cOptions.m_nMouseScrollSpeed > 50) {
        m_cOptions.m_nMouseScrollSpeed = 28;
    }

    m_cOptions.m_nGuiFeedbackLevel = GetPrivateProfileIntA("Game Options",
        "GUI Feedback Level",
        5,
        g_pBaldurChitin->GetIniFileName());

    if (m_cOptions.m_nGuiFeedbackLevel == 0 || m_cOptions.m_nGuiFeedbackLevel > 5) {
        m_cOptions.m_nGuiFeedbackLevel = 5;
    }

    m_cOptions.m_nLocatorFeedbackLevel = GetPrivateProfileIntA("Game Options",
        "Locator Feedback Level",
        3,
        g_pBaldurChitin->GetIniFileName());

    if (m_cOptions.m_nLocatorFeedbackLevel == 0 || m_cOptions.m_nLocatorFeedbackLevel > 5) {
        m_cOptions.m_nLocatorFeedbackLevel = 3;
    }

    m_cOptions.m_nBoredTime = GetPrivateProfileIntA("Game Options",
        "Bored Timeout",
        CScreenWorld::BORED_TIME,
        g_pBaldurChitin->GetIniFileName());

    if (m_cOptions.m_nBoredTime == 0 || m_cOptions.m_nBoredTime > CScreenWorld::BORED_TIME) {
        m_cOptions.m_nBoredTime = CScreenWorld::BORED_TIME;
    }

    m_cOptions.m_bAlwaysDither = GetPrivateProfileIntA("Game Options",
        "Always Dither",
        1,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.m_bSubtitles = GetPrivateProfileIntA("Game Options",
        "Subtitles",
        0,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.m_nKeyboardScrollSpeed = GetPrivateProfileIntA("Game Options",
        "Keyboard Scroll Speed",
        36,
        g_pBaldurChitin->GetIniFileName());

    if (m_cOptions.m_nKeyboardScrollSpeed < 18 || m_cOptions.m_nKeyboardScrollSpeed > 150) {
        m_cOptions.m_nKeyboardScrollSpeed = 36;
    }

    m_cOptions.m_nCommandSoundsFrequency = GetPrivateProfileIntA("Game Options",
        "Command Sounds Frequency",
        2,
        g_pBaldurChitin->GetIniFileName());

    if (m_cOptions.m_nCommandSoundsFrequency == 0 || m_cOptions.m_nCommandSoundsFrequency > 3) {
        m_cOptions.m_nCommandSoundsFrequency = 2;
    }

    m_cOptions.m_nSelectionSoundsFrequency = GetPrivateProfileIntA("Game Options",
        "Selection Sounds Frequency",
        3,
        g_pBaldurChitin->GetIniFileName());

    if (m_cOptions.m_nSelectionSoundsFrequency == 0 || m_cOptions.m_nSelectionSoundsFrequency > 3) {
        m_cOptions.m_nSelectionSoundsFrequency = 3;
    }

    m_cOptions.m_nEffectTextLevel = GetPrivateProfileIntA("Game Options",
        "Effect Text Level",
        62,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.m_bDarkvision = GetPrivateProfileIntA("Game Options",
        "Darkvision",
        0,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.m_bWeatherEnabled = GetPrivateProfileIntA("Game Options",
        "Weather",
        1,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.m_nTutorialState = GetPrivateProfileIntA("Game Options",
        "Tutorial State",
        1,
        g_pBaldurChitin->GetIniFileName());

    if (m_cOptions.m_nTutorialState > 1) {
        m_cOptions.m_nTutorialState = 1;
    }

    m_cOptions.m_bAttackSounds = GetPrivateProfileIntA("Game Options",
        "Attack Sounds",
        1,
        g_pBaldurChitin->GetIniFileName());

    if (m_cOptions.m_bAttackSounds > 1) {
        m_cOptions.m_bAttackSounds = 1;
    }

    m_cOptions.m_nAutoPauseState = GetPrivateProfileIntA("Game Options",
        "Auto Pause State",
        0,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.m_bAutoPauseCenter = GetPrivateProfileIntA("Game Options",
        "Auto Pause Center",
        1,
        g_pBaldurChitin->GetIniFileName());

    if (g_pBaldurChitin->cNetwork.GetSessionHosting() == TRUE) {
        m_cOptions.m_nDifficultyLevel = GetPrivateProfileIntA("Game Options",
            "Difficulty Level",
            3,
            g_pBaldurChitin->GetIniFileName());

        if (m_cOptions.m_nDifficultyLevel < 1 || m_cOptions.m_nDifficultyLevel > 5) {
            m_cOptions.m_nDifficultyLevel = 3;
        }

        switch (m_cOptions.m_nDifficultyLevel) {
        case 1:
            m_cOptions.m_nDifficultyMultiplier = -50;
            break;
        case 2:
            m_cOptions.m_nDifficultyMultiplier = -25;
            break;
        case 3:
            m_cOptions.m_nDifficultyMultiplier = 0;
            break;
        case 4:
            m_cOptions.m_nDifficultyMultiplier = 50;
            break;
        case 5:
            m_cOptions.m_nDifficultyMultiplier = 100;
            break;
        }

        m_cOptions.m_nMPDifficultyMultiplier = m_cOptions.m_nDifficultyMultiplier;
        m_nDifficultyLevel = -1;
    } else {
        m_nDifficultyLevel = GetPrivateProfileIntA("Game Options",
            "Difficulty Level",
            3,
            g_pBaldurChitin->GetIniFileName());

        // NOTE: Difficulty level is not normalized to 1..5 range as in the
        // code path above.

        m_cOptions.m_nDifficultyLevel = m_nDifficultyLevel;
    }

    m_cOptions.m_bQuickItemMapping = GetPrivateProfileIntA("Game Options",
        "Quick Item Mapping",
        1,
        g_pBaldurChitin->GetIniFileName());

    if (m_cOptions.m_bQuickItemMapping > 1) {
        m_cOptions.m_bQuickItemMapping = 1;
    }

    m_cOptions.m_bTerrainHugging = GetPrivateProfileIntA("Game Options",
        "Terrain Hugging",
        0,
        g_pBaldurChitin->GetIniFileName());

    CGameSprite::SHOW_CHARACTER_HP = GetPrivateProfileIntA("Game Options",
        "Show Character HP",
        0,
        g_pBaldurChitin->GetIniFileName());

    CGameSprite::GRAVITY_IS_DOWN = GetPrivateProfileIntA("Game Options",
        "Gravity Is Down",
        0,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.m_bShowQuestXP = GetPrivateProfileIntA("Game Options",
        "Show Quest XP",
        0,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.m_bStaticAnimations = GetPrivateProfileIntA("Game Options",
        "Static Animations",
        1,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.m_bCriticalHitScreenShake = GetPrivateProfileIntA("Game Options",
        "Critical Hit Screen Shake",
        1,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.m_bSuppressExtraDifficultyDamage = GetPrivateProfileIntA("Game Options",
        "Suppress Extra Difficulty Damage",
        0,
        g_pBaldurChitin->GetIniFileName());

    if (!g_pBaldurChitin->cNetwork.GetSessionHosting()) {
        g_pBaldurChitin->m_cBaldurMessage.DemandSettingsNightmareMode(FALSE);
    }

    m_cOptions.m_nDefaultCharGenPointsPool = GetPrivateProfileIntA("Game Options",
        "DefaultCharGenPointsPool",
        16,
        g_pBaldurChitin->GetIniFileName());

    if (m_cOptions.m_nDefaultCharGenPointsPool < 10 || m_cOptions.m_nDefaultCharGenPointsPool > 108) {
        m_cOptions.m_nDefaultCharGenPointsPool = 16;
    }

    m_cOptions.m_nDefaultCharGenPoints = GetPrivateProfileIntA("Game Options",
        "DefaultCharGenPoints",
        10,
        g_pBaldurChitin->GetIniFileName());

    if (m_cOptions.m_nDefaultCharGenPoints < 3 || m_cOptions.m_nDefaultCharGenPoints > 18) {
        m_cOptions.m_nDefaultCharGenPoints = 10;
    }

    m_cOptions.m_bHotkeysOnToolTips = GetPrivateProfileIntA("Game Options",
        "Hotkeys On Tooltips",
        1,
        g_pBaldurChitin->GetIniFileName());

    g_pBaldurChitin->m_cBaldurMessage.SetSignalDefaultSecondsToTimeout();

    m_cOptions.m_bAllScreenShake = GetPrivateProfileIntA("Game Options",
        "All Screen Shake",
        1,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.m_nOldPortraitHealth = GetPrivateProfileIntA("Game Options",
        "Old Portrait Health",
        0,
        g_pBaldurChitin->GetIniFileName());

    m_cOptions.m_nHitPointBarSequence = GetPrivateProfileIntA("Game Options",
        "Hit Point Bar Sequence",
        0,
        g_pBaldurChitin->GetIniFileName());

    if (m_cOptions.m_nHitPointBarSequence < 0 || m_cOptions.m_nHitPointBarSequence > 5) {
        m_cOptions.m_nHitPointBarSequence = 0;
    }
}

// 0x5AA350
void CInfGame::SaveOptions()
{
    CString sValue;

    sValue.Format("%d", m_cOptions.m_bFootStepsSounds);
    WritePrivateProfileStringA("Game Options", "Footsteps", sValue, g_pBaldurChitin->GetIniFileName());

    if ((!g_pChitin->cNetwork.GetSessionOpen() || g_pChitin->cNetwork.GetSessionHosting())
        && m_cOptions.m_nGoreOption != 0) {
        sValue.Format("%d", m_cOptions.m_nGoreOption);
        WritePrivateProfileStringA("Game Options", "Grant 2000", sValue, g_pBaldurChitin->GetIniFileName());

        sValue.Format("%d", m_cOptions.m_nGore);
        WritePrivateProfileStringA("Game Options", "Chucknomitron", sValue, g_pBaldurChitin->GetIniFileName());
    }

    sValue.Format("%d", m_cOptions.m_nMouseScrollSpeed);
    WritePrivateProfileStringA("Game Options", "Mouse Scroll Speed", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_nGuiFeedbackLevel);
    WritePrivateProfileStringA("Game Options", "GUI Feedback Level", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_nLocatorFeedbackLevel);
    WritePrivateProfileStringA("Game Options", "Locator Feedback Level", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_nBoredTime);
    WritePrivateProfileStringA("Game Options", "Bored Timeout", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bAlwaysDither);
    WritePrivateProfileStringA("Game Options", "Always Dither", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bSubtitles);
    WritePrivateProfileStringA("Game Options", "Subtitles", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_nKeyboardScrollSpeed);
    WritePrivateProfileStringA("Game Options", "Keyboard Scroll Speed", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_nCommandSoundsFrequency);
    WritePrivateProfileStringA("Game Options", "Command Sounds Frequency", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_nSelectionSoundsFrequency);
    WritePrivateProfileStringA("Game Options", "Selection Sounds Frequency", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_nEffectTextLevel);
    WritePrivateProfileStringA("Game Options", "Effect Text Level", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bDarkvision);
    WritePrivateProfileStringA("Game Options", "Darkvision", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bWeatherEnabled);
    WritePrivateProfileStringA("Game Options", "Weather", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_nTutorialState);
    WritePrivateProfileStringA("Game Options", "Tutorial State", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bAttackSounds);
    WritePrivateProfileStringA("Game Options", "Attack Sounds", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_nAutoPauseState);
    WritePrivateProfileStringA("Game Options", "Auto Pause State", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bAutoPauseCenter);
    WritePrivateProfileStringA("Game Options", "Auto Pause Center", sValue, g_pBaldurChitin->GetIniFileName());

    if (m_nDifficultyLevel == -1) {
        sValue.Format("%d", m_cOptions.m_nDifficultyLevel);
    } else {
        sValue.Format("%d", m_nDifficultyLevel);
    }

    WritePrivateProfileStringA("Game Options", "Difficulty Level", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bQuickItemMapping);
    WritePrivateProfileStringA("Game Options", "Quick Item Mapping", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bEnvironmentalAudio);
    WritePrivateProfileStringA("Game Options", "Environmental Audio", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bTerrainHugging);
    WritePrivateProfileStringA("Game Options", "Terrain Hugging", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bStaticAnimations);
    WritePrivateProfileStringA("Game Options", "Static Animations", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bTranslucentBlts);
    WritePrivateProfileStringA("Game Options", "Translucent Blts", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bMaxHP);
    WritePrivateProfileStringA("Game Options", "Maximum HP", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bAutoPauseOnTrap);
    WritePrivateProfileStringA("Game Options", "Pause On Trap", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", CGameSprite::SHOW_CHARACTER_HP);
    WritePrivateProfileStringA("Game Options", "Show Character HP", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bShowQuestXP);
    WritePrivateProfileStringA("Game Options", "Show Quest XP", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", g_pBaldurChitin->field_1A0);
    WritePrivateProfileStringA("Program Options", "Double Byte Character Support", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bCriticalHitScreenShake);
    WritePrivateProfileStringA("Game Options", "Critical Hit Screen Shake", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bHotkeysOnToolTips);
    WritePrivateProfileStringA("Game Options", "Hotkeys On Tooltips", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bDuplicateFloatingText);
    WritePrivateProfileStringA("Game Options", "Duplicate Floating Text", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_nTilesPrecachePercent);
    WritePrivateProfileStringA("Game Options", "Tiles Precache Percent", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bSuppressExtraDifficultyDamage);
    WritePrivateProfileStringA("Game Options", "Suppress Extra Difficulty Damage", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_bAllScreenShake);
    WritePrivateProfileStringA("Game Options", "All Screen Shake", sValue, g_pBaldurChitin->GetIniFileName());

    sValue.Format("%d", m_cOptions.m_nHitPointBarSequence);
    WritePrivateProfileStringA("Game Options", "Hit Point Bar Sequence", sValue, g_pBaldurChitin->GetIniFileName());
}

// 0x5AAC30
void CInfGame::LoadMultiPlayerPermissions()
{
    if (!g_pBaldurChitin->cNetwork.GetSessionOpen()
        || !g_pBaldurChitin->cNetwork.GetSessionHosting()) {
        return;
    }

    CMultiplayerSettings* pSettings = GetMultiplayerSettings();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 8323
    UTIL_ASSERT(pSettings != NULL);

    CString sPermissions;
    CString sDefault("");

    GetPrivateProfileStringA("Multiplayer",
        "Default Permissions",
        sDefault,
        sPermissions.GetBuffer(10),
        10,
        g_pBaldurChitin->GetIniFileName());
    sPermissions.ReleaseBuffer(-1);

    if (sPermissions.GetLength() == 8) {
        BOOL bPurchasing = sPermissions[0] == '1';
        BOOL bAreaTransition = sPermissions[1] == '1';
        BOOL bDialog = sPermissions[2] == '1';
        BOOL bCharRecords = sPermissions[3] == '1';
        BOOL bPausing = sPermissions[4] == '1';
        BOOL bGroupPool = sPermissions[5] == '1';
        BOOL bLeader = sPermissions[6] == '1';
        BOOL bModifyChars = sPermissions[7] == '1';

        pSettings->SetPermission(-1,
            CGamePermission::PURCHASING,
            bPurchasing,
            TRUE);
        pSettings->SetPermission(-1,
            CGamePermission::AREA_TRANSITION,
            bAreaTransition,
            TRUE);
        pSettings->SetPermission(-1,
            CGamePermission::DIALOG,
            bDialog,
            TRUE);
        pSettings->SetPermission(-1,
            CGamePermission::CHAR_RECORDS,
            bCharRecords,
            TRUE);
        pSettings->SetPermission(-1,
            CGamePermission::PAUSING,
            bPausing,
            TRUE);
        pSettings->SetPermission(-1,
            CGamePermission::GROUP_POOL,
            bGroupPool,
            TRUE);
        pSettings->SetPermission(-1,
            CGamePermission::LEADER,
            bLeader,
            TRUE);
        pSettings->SetPermission(-1,
            CGamePermission::MODIFY_CHARS,
            bModifyChars,
            TRUE);
    }

    INT nRestrictStore = GetPrivateProfileIntA("Multiplayer",
        "Restrict Stores",
        0,
        g_pBaldurChitin->GetIniFileName());
    pSettings->SetRestrictStoreOption(nRestrictStore != 0);

    BYTE nImportingBitField = static_cast<BYTE>(GetPrivateProfileIntA("Multiplayer",
        "Import Character",
        CMultiplayerSettings::IMPORT_ALL,
        g_pBaldurChitin->GetIniFileName()));
    pSettings->SetImportingCharacterOption(nImportingBitField);
}

// 0x5AAEC0
void CInfGame::SaveMultiPlayerPermissions()
{
    if (!g_pBaldurChitin->cNetwork.GetSessionOpen()
        || !g_pBaldurChitin->cNetwork.GetSessionHosting()) {
        return;
    }

    CMultiplayerSettings* pSettings = GetMultiplayerSettings();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 3290
    UTIL_ASSERT(pSettings != NULL);

    BOOL bPurchasing = pSettings->GetPermission(-1, CGamePermission::PURCHASING);
    BOOL bAreaTransition = pSettings->GetPermission(-1, CGamePermission::AREA_TRANSITION);
    BOOL bDialog = pSettings->GetPermission(-1, CGamePermission::DIALOG);
    BOOL bCharRecords = pSettings->GetPermission(-1, CGamePermission::CHAR_RECORDS);
    BOOL bPausing = pSettings->GetPermission(-1, CGamePermission::PAUSING);
    BOOL bGroupPool = pSettings->GetPermission(-1, CGamePermission::GROUP_POOL);
    BOOL bLeader = pSettings->GetPermission(-1, CGamePermission::LEADER);
    BOOL bModifyChars = pSettings->GetPermission(-1, CGamePermission::MODIFY_CHARS);

    CString sPermissions("00000000");
    sPermissions.SetAt(0, bPurchasing + '0');
    sPermissions.SetAt(1, bAreaTransition + '0');
    sPermissions.SetAt(2, bDialog + '0');
    sPermissions.SetAt(3, bCharRecords + '0');
    sPermissions.SetAt(4, bPausing + '0');
    sPermissions.SetAt(5, bGroupPool + '0');
    sPermissions.SetAt(6, bLeader + '0');
    sPermissions.SetAt(7, bModifyChars + '0');

    WritePrivateProfileStringA("Multiplayer",
        "Default Permissions",
        sPermissions,
        g_pBaldurChitin->GetIniFileName());

    CString sTemp;

    sTemp.Format("%d", pSettings->m_bRestrictStoreOption);
    WritePrivateProfileStringA("Multiplayer",
        "Restrict Stores",
        sTemp,
        g_pBaldurChitin->GetIniFileName());

    sTemp.Format("%d", pSettings->m_nImportingBitField);
    WritePrivateProfileStringA("Multiplayer",
        "Import Character",
        sTemp,
        g_pBaldurChitin->GetIniFileName());
}

// 0x5AB190
void CInfGame::LoadGame(BOOLEAN bProgressBarRequired, BOOLEAN bProgressBarInPlace)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 8447
    UTIL_ASSERT(m_sSaveGame != "");

    g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->sub_518650();
    g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->sub_518690();

    g_pBaldurChitin->GetTlkTable().m_override.CloseFiles();

    dword_8E52EC = 0;
    dword_8E52F0 = 0;

    if (!bProgressBarInPlace && bProgressBarRequired == TRUE) {
        sub_59FA00(TRUE);
        g_pChitin->SetProgressBar(TRUE,
            9888,
            0,
            0,
            FALSE,
            0,
            FALSE,
            0,
            FALSE,
            FALSE,
            255);
        g_pChitin->cProgressBar.m_nActionProgress = 0;
        g_pChitin->cProgressBar.m_nActionTarget = 8000000;
        g_pChitin->cProgressBar.m_bDisableMinibars = 1;
    }

    g_pChitin->m_bDisplayStale = TRUE;
    SleepEx(25, FALSE);

    if (bProgressBarInPlace || bProgressBarRequired) {
        g_pChitin->cProgressBar.AddActionTarget(625000);
    }

    m_bInLoadGame = TRUE;

    BOOL bMusicThreadPriorityChanged = FALSE;
    int nMusicThreadPriority = GetThreadPriority(g_pChitin->m_hMusicThread);

    if (GetPrivateProfileIntA("Program Options", "Volume Music", 0, g_pChitin->GetIniFileName())) {
        bMusicThreadPriorityChanged = SetThreadPriority(g_pChitin->m_hMusicThread, 15);
    }

    InitGame(FALSE, FALSE);

    if (bProgressBarInPlace || bProgressBarRequired) {
        ProgressBarCallback(312500, FALSE);
    }

    m_bFromNewGame = FALSE;

    CString sDir = GetDirSave();
    if (sDir == m_sSaveDir + "default\\") {
        sDir = m_sMultiplayerSaveDir + m_sSaveGame + "\\";
    }

    if (!g_pChitin->cDimm.DirectoryCopyFiles(sDir, m_sTempDir)) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 8524
        UTIL_ASSERT_MSG(FALSE, "Couldn't copy files from Temp to Save directories");
    }

    if (!g_pChitin->cDimm.UncompressDirectory(m_sTempDir, sDir)) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 8526
        UTIL_ASSERT_MSG(FALSE, "Couldn't uncompress save game into Temp directory");
    }

    if (bProgressBarInPlace || bProgressBarRequired) {
        ProgressBarCallback(312500, FALSE);
    }

    if (!DeleteFileA(m_sTempDir + "ICEWIND2.BMP")
        && GetLastError() != ERROR_FILE_NOT_FOUND) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 8541
        UTIL_ASSERT(FALSE);
    }

    if (!DeleteFileA(m_sTempDir + "PORTRT0.BMP")
        && GetLastError() != ERROR_FILE_NOT_FOUND) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 8548
        UTIL_ASSERT(FALSE);
    }

    if (!DeleteFileA(m_sTempDir + "PORTRT1.BMP")
        && GetLastError() != ERROR_FILE_NOT_FOUND) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 8555
        UTIL_ASSERT(FALSE);
    }

    if (!DeleteFileA(m_sTempDir + "PORTRT2.BMP")
        && GetLastError() != ERROR_FILE_NOT_FOUND) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 8562
        UTIL_ASSERT(FALSE);
    }

    if (!DeleteFileA(m_sTempDir + "PORTRT3.BMP")
        && GetLastError() != ERROR_FILE_NOT_FOUND) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 8569
        UTIL_ASSERT(FALSE);
    }

    if (!DeleteFileA(m_sTempDir + "PORTRT4.BMP")
        && GetLastError() != ERROR_FILE_NOT_FOUND) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 8576
        UTIL_ASSERT(FALSE);
    }

    if (!DeleteFileA(m_sTempDir + "PORTRT5.BMP")
        && GetLastError() != ERROR_FILE_NOT_FOUND) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 8583
        UTIL_ASSERT(FALSE);
    }

    g_pChitin->cDimm.AddToDirectoryList(m_sTempDir, TRUE);

    g_pBaldurChitin->GetTlkTable().OpenOverride(CString("temp/default.toh"),
        CString("temp/default.tot"));

    m_cWorldMap.SetResRef(CResRef("WORLDMAP"));

    CGameFile cGameFile;
    cGameFile.SetResRef(CResRef("ICEWIND2"), TRUE, TRUE);

    Unmarshal(cGameFile.GetData(),
        cGameFile.GetDataSize(),
        bProgressBarInPlace | bProgressBarRequired);

    g_pBaldurChitin->cSoundMixer.StartSong(-1, 0x1 | 0x2);
    SleepEx(500, FALSE);
    g_pBaldurChitin->cSoundMixer.StopMusic(TRUE);

    cGameFile.Release();

    m_bAnotherPlayerJoinedGame = FALSE;

    if (g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
        m_cRemoteObjectArray.ChangeControlOnLoadGame();
    }

    m_bInLoadGame = FALSE;

    if (bMusicThreadPriorityChanged) {
        SetThreadPriority(g_pChitin->m_hMusicThread, nMusicThreadPriority);
    }

    m_nLastSaveTime = m_worldTime.m_gameTime;

    if (!bProgressBarInPlace && bProgressBarRequired == TRUE) {
        g_pChitin->cProgressBar.m_nActionProgress = g_pChitin->cProgressBar.m_nActionTarget;
        g_pChitin->cProgressBar.m_bDisableMinibars = TRUE;

        g_pChitin->m_bDisplayStale = TRUE;
        sub_59FA00(TRUE);

        g_pChitin->SetProgressBar(FALSE,
            0,
            0,
            0,
            FALSE,
            0,
            FALSE,
            0,
            FALSE,
            FALSE,
            255);
    }

    SelectAll(FALSE);

    g_pBaldurChitin->GetObjectGame()->m_cButtonArray.UpdateState();
}

// 0x5ABA20
void CInfGame::NewGame(BOOLEAN bProgressBarRequired, BOOLEAN bProgressBarInPlace)
{
    if (!bProgressBarInPlace && bProgressBarRequired == TRUE) {
        sub_59FA00(TRUE);

        g_pChitin->SetProgressBar(TRUE,
            9889,
            0,
            0,
            FALSE,
            0,
            FALSE,
            0,
            FALSE,
            FALSE,
            255);
        g_pChitin->cProgressBar.m_nActionProgress = 0;
        g_pChitin->cProgressBar.m_nActionTarget = 8000000;
        g_pChitin->cProgressBar.m_bDisableMinibars = TRUE;
    }

    if (bProgressBarInPlace || bProgressBarRequired) {
        g_pChitin->cProgressBar.AddActionTarget(625000);
    }

    m_bInLoadGame = TRUE;

    BOOL bMusicThreadPriorityChanged = FALSE;
    int nMusicThreadPriority = GetThreadPriority(g_pChitin->m_hMusicThread);

    if (GetPrivateProfileIntA("Program Options", "Volume Music", 0, g_pChitin->GetIniFileName())) {
        bMusicThreadPriorityChanged = SetThreadPriority(g_pChitin->m_hMusicThread, 15);
    }

    g_pBaldurChitin->GetTlkTable().m_override.CloseFiles();

    if (g_pChitin->cDimm.DirectoryRemoveFiles(CString(".\\mpsave\\default\\"))) {
        g_pChitin->cDimm.m_cKeyTable.RescanEverything();
    }

    InitGame(FALSE, FALSE);

    if (bProgressBarInPlace || bProgressBarRequired) {
        ProgressBarCallback(156250, FALSE);
    }

    CVariable chapter;
    chapter.SetName(CHAPTER_GLOBAL);
    chapter.m_intValue = -1;
    m_variables.AddKey(chapter);

    m_bFromNewGame = TRUE;

    g_pChitin->cDimm.AddToDirectoryList(m_sScriptsDir, TRUE);
    g_pChitin->cDimm.AddToDirectoryList(m_sSoundsDir, TRUE);
    g_pChitin->cDimm.AddToDirectoryList(m_sPortraitsDir, TRUE);
    g_pChitin->cDimm.AddToDirectoryList(m_sCharactersDir, TRUE);
    g_pChitin->cDimm.AddToDirectoryList(m_sTempDir, TRUE);

    g_pBaldurChitin->GetTlkTable().OpenOverride(CString("temp/default.toh"), CString("temp/default.tot"));

    if (bProgressBarInPlace || bProgressBarRequired) {
        ProgressBarCallback(156250, FALSE);
    }

    m_cWorldMap.SetResRef(CResRef("WORLDMAP"));

    CGameFile cGameFile;
    cGameFile.SetResRef(CResRef("ICEWIND2"), TRUE, TRUE);

    Unmarshal(cGameFile.GetData(),
        cGameFile.GetDataSize(),
        bProgressBarInPlace | bProgressBarRequired);

    m_cOptions.m_nNightmareMode = GetPrivateProfileIntA("Game Options",
        "Nightmare Mode",
        0,
        g_pBaldurChitin->GetIniFileName());

    if (m_cOptions.m_nNightmareMode == 1 && m_cOptions.m_nDifficultyLevel != 5) {
        m_cOptions.m_nDifficultyLevel = 5;
    }

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 8769
    UTIL_ASSERT(m_nCharacters == 0);

    m_bAnotherPlayerJoinedGame = FALSE;

    m_sSaveGame = "default";
    CScreenCharacter::SAVE_NAME = "default";

    m_bInLoadGame = FALSE;

    if (bMusicThreadPriorityChanged == TRUE) {
        SetThreadPriority(g_pChitin->m_hMusicThread, nMusicThreadPriority);
    }

    if (bProgressBarInPlace || bProgressBarRequired) {
        ProgressBarCallback(156250, FALSE);
    }

    if (!bProgressBarInPlace && bProgressBarRequired == TRUE) {
        g_pChitin->cProgressBar.m_nActionProgress = g_pChitin->cProgressBar.m_nActionTarget;
        g_pChitin->cProgressBar.m_bDisableMinibars = TRUE;
        g_pChitin->m_bDisplayStale = TRUE;

        sub_59FA00(TRUE);

        g_pChitin->SetProgressBar(FALSE,
            0,
            0,
            0,
            FALSE,
            0,
            FALSE,
            0,
            FALSE,
            FALSE,
            255);
    }

    m_nReputation = -10;
}

// 0x5AC0A0
BOOLEAN CInfGame::sub_5AC0A0(CGameArea* pArea)
{
    if (!m_bInLoadGame
        && pArea != NULL
        && pArea->m_bAreaLoaded) {
        return pArea->field_B16;
    } else {
        return FALSE;
    }
}

// 0x5AC0D0
void CInfGame::sub_5AC0D0()
{
    STRREF strError;

    for (int index = 0; index < CINFGAME_MAX_AREAS; index++) {
        if (m_gameAreas[index] != NULL && m_gameAreas[index]->m_bAreaLoaded) {
            m_gameAreas[index]->field_B16 = !m_gameAreas[index]->CanSaveGame(strError);
        }
    }
}

// 0x5AD470
void CInfGame::SelectAll(BOOLEAN bPlaySound)
{
    LONG iSprite;
    CGameSprite* pSprite;
    BYTE rc;
    SHORT cnt;
    POSITION pos;

    for (cnt = 0; cnt < m_nCharacters; cnt++) {
        iSprite = m_characterPortraits[cnt];
        do {
            rc = m_cObjectArray.GetShare(iSprite,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            if (pSprite->m_pArea == GetVisibleArea()) {
                if (pSprite->Orderable(FALSE) && !pSprite->m_bSelected) {
                    do {
                        rc = m_cObjectArray.GetDeny(iSprite,
                            CGameObjectArray::THREAD_ASYNCH,
                            reinterpret_cast<CGameObject**>(&pSprite),
                            INFINITE);
                    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                    if (rc == CGameObjectArray::SUCCESS) {
                        pSprite->Select();
                        m_cObjectArray.ReleaseDeny(iSprite,
                            CGameObjectArray::THREAD_ASYNCH,
                            INFINITE);
                        UpdatePortrait(cnt, 1);
                    }
                }
                if (pSprite->Orderable(FALSE) && bPlaySound) {
                    bPlaySound = FALSE;
                    pSprite->PlaySound(CGameSprite::SOUND_SELECT, TRUE, FALSE, FALSE);
                }
            } else if (pSprite->m_bSelected) {
                do {
                    rc = m_cObjectArray.GetDeny(iSprite,
                        CGameObjectArray::THREAD_ASYNCH,
                        reinterpret_cast<CGameObject**>(&pSprite),
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                if (rc == CGameObjectArray::SUCCESS) {
                    pSprite->Unselect();
                    m_cObjectArray.ReleaseDeny(iSprite,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                    UpdatePortrait(cnt, 1);
                }
            }
            m_cObjectArray.ReleaseShare(iSprite,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    pos = m_familiars.GetHeadPosition();
    while (pos != NULL) {
        iSprite = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_familiars.GetNext(pos)));
        rc = m_cObjectArray.GetShare(iSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            if (pSprite->GetArea() == GetVisibleArea()) {
                if (pSprite->Orderable(FALSE) && !pSprite->m_bSelected) {
                    do {
                        rc = m_cObjectArray.GetDeny(iSprite,
                            CGameObjectArray::THREAD_ASYNCH,
                            reinterpret_cast<CGameObject**>(&pSprite),
                            INFINITE);
                    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                    if (rc == CGameObjectArray::SUCCESS) {
                        pSprite->Select();
                        m_cObjectArray.ReleaseDeny(iSprite,
                            CGameObjectArray::THREAD_ASYNCH,
                            INFINITE);
                    }
                }
                if (pSprite->Orderable(FALSE) && bPlaySound) {
                    bPlaySound = FALSE;
                    pSprite->PlaySound(CGameSprite::SOUND_SELECT, TRUE, FALSE, FALSE);
                }
            }
            rc = m_cObjectArray.ReleaseShare(iSprite,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    pos = m_allies.GetHeadPosition();
    while (pos != NULL) {
        iSprite = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_allies.GetNext(pos)));
        rc = m_cObjectArray.GetShare(iSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            if (pSprite->GetArea() == GetVisibleArea()) {
                if (pSprite->Orderable(FALSE) && !pSprite->m_bSelected) {
                    do {
                        rc = m_cObjectArray.GetDeny(iSprite,
                            CGameObjectArray::THREAD_ASYNCH,
                            reinterpret_cast<CGameObject**>(&pSprite),
                            INFINITE);
                    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                    if (rc == CGameObjectArray::SUCCESS) {
                        pSprite->Select();
                        m_cObjectArray.ReleaseDeny(iSprite,
                            CGameObjectArray::THREAD_ASYNCH,
                            INFINITE);
                    }
                }
                if (pSprite->Orderable(FALSE) && bPlaySound) {
                    bPlaySound = FALSE;
                    pSprite->PlaySound(CGameSprite::SOUND_SELECT, TRUE, FALSE, FALSE);
                }
            }
            rc = m_cObjectArray.ReleaseShare(iSprite,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    SelectToolbar();
}

// 0x5AD7E0
void CInfGame::UnselectAll()
{
    LONG* pGroup = m_group.GetGroupList();
    SHORT nGroup = m_group.GetCount();
    CGameSprite* pSprite;

    for (SHORT cnt = 0; cnt < nGroup; cnt++) {
        BYTE rc;
        do {
            rc = m_cObjectArray.GetDeny(pGroup[cnt],
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            pSprite->Unselect();

            m_cObjectArray.ReleaseDeny(pGroup[cnt],
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            // NOTE: Uninline.
            UpdatePortrait(GetFixedOrderCharacterPortraitNum(pGroup[cnt]), 1);
        }
    }

    delete pGroup;
}

// 0x5AD8A0
BOOL CInfGame::SelectCharacter(LONG characterId, BOOLEAN bPlaySelectSound)
{
    CGameSprite* pSprite;
    BYTE rc;
    SHORT nPortrait = GetCharacterPortraitNum(characterId);

    if (nPortrait == -1) {
        if (!IsAlly(characterId) || !IsFamiliar(characterId)) {
            return FALSE;
        }
    }

    do {
        rc = m_cObjectArray.GetShare(characterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    if (pSprite->m_bSelected
        && !g_pBaldurChitin->GetScreenWorld()->GetShiftKey()) {
        do {
            rc = m_cObjectArray.GetDeny(characterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            pSprite->Unselect();

            m_cObjectArray.ReleaseDeny(characterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    } else if ((GetVisibleArea() == pSprite->GetArea()
                   || GetGroup()->GetCount() == 0)
        && pSprite->Orderable(FALSE)
        && pSprite->GetArea() != NULL) {
        if (GetVisibleArea() != pSprite->GetArea()) {
            if (GetVisibleArea() != NULL) {
                GetVisibleArea()->m_bPicked = FALSE;
                GetVisibleArea()->m_iPicked = -1;
                GetVisibleArea()->m_nToolTip = 0;
                GetVisibleArea()->OnDeactivation();
            }

            m_visibleArea = pSprite->GetArea()->GetId();

            GetVisibleArea()->OnActivation();
        }

        do {
            rc = m_cObjectArray.GetDeny(characterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            pSprite->Select();

            m_cObjectArray.ReleaseDeny(characterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    if (bPlaySelectSound && pSprite->Orderable(FALSE)) {
        pSprite->PlaySound(CGameSprite::SOUND_SELECT, TRUE, FALSE, FALSE);
    }

    m_cObjectArray.ReleaseShare(characterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    if (nPortrait != -1) {
        UpdatePortrait(nPortrait, 1);
    }

    return TRUE;
}

// 0x5ADAE0
void CInfGame::SelectToolbar()
{
    m_nState = 0;
    m_cButtonArray.m_nSelectedButton = 100;

    switch (m_group.GetCount()) {
    case 0:
        m_cButtonArray.SetState(0, 0);
        break;
    case 1: {
        LONG* pGroupList = m_group.GetGroupList();
        if (pGroupList == NULL) {
            m_cButtonArray.SetState(0, 0);
            delete[] pGroupList;
            return;
        }

        CGameSprite* pSprite;
        BYTE rc;
        do {
            rc = m_cObjectArray.GetShare(pGroupList[0],
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            m_cButtonArray.SetState(0, 0);
            delete[] pGroupList;
            return;
        }

        if (IcewindMisc::IsPC(pSprite) == TRUE) {
            m_cButtonArray.UpdateCustomizableButtonTypes(pSprite->field_3D14);
            m_cButtonArray.UpdateSelectedWeaponSet(pSprite->m_nWeaponSet);
            m_cButtonArray.SetState(114, 0);
        } else {
            m_cButtonArray.UpdateSelectedWeaponSet(pSprite->m_nWeaponSet);
            m_cButtonArray.SetState(110, 0);
        }

        m_cObjectArray.ReleaseShare(pGroupList[0],
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        delete[] pGroupList;
        break;
    }
    default:
        m_cButtonArray.SetState(110, 0);
        break;
    }

    // NOTE: 0x691130 (CScreenWorld's active panel id, not in our tree yet)
    // returns `field_EA4`.
    if (g_pBaldurChitin->m_pEngineWorld->field_EA4 == 8) {
        g_pBaldurChitin->m_pEngineWorld->StopContainer();
    }
}

// 0x5ADC90
void CInfGame::DemandServerStore(const CResRef& store, BOOL bSaveToDisk)
{
    int cnt;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 9987
    UTIL_ASSERT(g_pChitin->cNetwork.GetSessionOpen() && g_pChitin->cNetwork.GetSessionHosting());

    for (cnt = 0; cnt < 12; cnt++) {
        if (m_aServerStore[cnt] != NULL
            && store == m_aServerStore[cnt]->m_resRef) {
            if (bSaveToDisk) {
                m_aServerStore[cnt]->Marshal(m_sTempDir);
            }

            m_nServerStoreDemands[cnt]++;

            return;
        }
    }

    for (cnt = 0; cnt < 12; cnt++) {
        if (m_aServerStore[cnt] == NULL) {
            m_aServerStore[cnt] = new CStore(store);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 10007
            UTIL_ASSERT(m_aServerStore[cnt] != NULL);

            if (m_aServerStore[cnt]->m_resRef == "") {
                delete m_aServerStore[cnt];
                m_aServerStore[cnt] = NULL;
            } else {
                m_nServerStoreDemands[cnt] = 1;
            }

            return;
        }
    }

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 10019
    UTIL_ASSERT(FALSE);
}

// 0x5ADE90
void CInfGame::ReleaseServerStore(const CResRef& store)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 10042
    UTIL_ASSERT(g_pChitin->cNetwork.GetSessionOpen() && g_pChitin->cNetwork.GetSessionHosting());

    for (int cnt = 0; cnt < 12; cnt++) {
        if (m_aServerStore[cnt] != NULL
            && store == m_aServerStore[cnt]->m_resRef) {
            m_nServerStoreDemands[cnt]--;

            if (m_nServerStoreDemands[cnt] == 0) {
                m_aServerStore[cnt]->Marshal(m_sTempDir);

                delete m_aServerStore[cnt];

                m_aServerStore[cnt] = NULL;
            }
        }
    }
}

// 0x5ADF60
CStore* CInfGame::GetServerStore(const CResRef& store)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 10079
    UTIL_ASSERT(g_pChitin->cNetwork.GetSessionOpen() && g_pChitin->cNetwork.GetSessionHosting());

    for (int cnt = 0; cnt < 12; cnt++) {
        if (m_aServerStore[cnt] != NULL
            && store == m_aServerStore[cnt]->m_resRef) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 10084
            UTIL_ASSERT(m_nServerStoreDemands[cnt] > 0);

            return m_aServerStore[cnt];
        }
    }

    return NULL;
}

// 0x5AF160
void CInfGame::SwapCharacters(DWORD pos1, DWORD pos2)
{
    UTIL_ASSERT(pos1 < (DWORD)m_nCharacters && pos2 < (DWORD)m_nCharacters);

    LONG nId1 = m_characterPortraits[pos2];
    LONG nId2 = m_characterPortraits[pos1];
    m_characterPortraits[pos1] = nId1;
    m_characterPortraits[pos2] = nId2;

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = m_cObjectArray.GetDeny(nId2,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        // NOTE: Uninline.
        pSprite->FetchCommonStrings();

        UpdatePortraitToolTip(pos2, pSprite->GetNameRef());

        m_cObjectArray.ReleaseDeny(nId2, CGameObjectArray::THREAD_ASYNCH, INFINITE);

        do {
            rc = m_cObjectArray.GetDeny(nId1,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            // NOTE: Uninline.
            pSprite->FetchCommonStrings();

            UpdatePortraitToolTip(pos1, pSprite->GetNameRef());

            m_cObjectArray.ReleaseDeny(nId1, CGameObjectArray::THREAD_ASYNCH, INFINITE);

            if (pos1 == 0 || pos2 == 0) {
                // The new leader's "selected" voice. The original checks the
                // allocation and calls AddMessage(0, 0) on failure, which our
                // new cannot return and AddMessage cannot take.
                LONG nLeaderId = m_characterPortraits[0];
                CMessage* pMessage = new CMessagePlaySound(2, TRUE, FALSE, nLeaderId, nLeaderId);

                g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
            }
        }
    }
}

// 0x5AF360
void CInfGame::UpdatePortrait(SHORT nPortrait, DWORD dwPanelId)
{
    if (nPortrait != -1) {
        CBaldurEngine* pEngine = g_pBaldurChitin->GetActiveEngine();
        if (pEngine != g_pBaldurChitin->m_pEngineProjector
            && pEngine != g_pBaldurChitin->m_pEngineSinglePlayer
            && pEngine != g_pBaldurChitin->m_pEngineWorldMap
            && pEngine != g_pBaldurChitin->m_pEngineSave) {
            CUIManager* pManager = pEngine->GetManager();
            CUIPanel* pPanel = pManager->GetPanel(dwPanelId);
            if (pPanel != NULL) {
                CUIControlBase* pControl = pPanel->GetControl(nPortrait);
                if (pControl != NULL) {
                    pControl->InvalidateRect();
                }
            } else {
                // NOTE: Obtaining active engine second time, probably some
                // inlining. Looks similar to `CBaldurEngine::OnPortraitLClick`.
                CUIManager* pManager = g_pBaldurChitin->GetActiveEngine()->GetManager();
                CUIPanel* pPanel = pManager->GetPanel(1);
                if (pPanel != NULL) {
                    CUIControlBase* pControl = pPanel->GetControl(nPortrait);
                    if (pControl != NULL) {
                        pControl->InvalidateRect();
                    }
                }
            }
        }
    }
}

// 0x5AF420
void CInfGame::sub_5AF420(SHORT nPortrait, DWORD dwPanelId)
{
    if (nPortrait != -1) {
        CBaldurEngine* pEngine = g_pBaldurChitin->GetActiveEngine();
        if (pEngine != g_pBaldurChitin->m_pEngineProjector
            && pEngine != g_pBaldurChitin->m_pEngineSinglePlayer
            && pEngine != g_pBaldurChitin->m_pEngineWorldMap
            && pEngine != g_pBaldurChitin->m_pEngineSave) {
            CUIManager* pManager = pEngine->GetManager();
            CUIPanel* pPanel = pManager->GetPanel(dwPanelId);
            if (pPanel != NULL) {
                CUIControlBase* pControl = pPanel->GetControl(nPortrait);
                if (pControl != NULL) {
                    pControl->func_54();
                }
            } else {
                CUIManager* pManager = g_pBaldurChitin->GetActiveEngine()->GetManager();
                CUIPanel* pPanel = pManager->GetPanel(1);
                if (pPanel != NULL) {
                    CUIControlBase* pControl = pPanel->GetControl(nPortrait);
                    if (pControl != NULL) {
                        pControl->func_54();
                    }
                }
            }
        }
    }
}

// 0x5AF4E0
SHORT CInfGame::GetCharacterPortraitNum(LONG nCharacterId)
{
    if (nCharacterId != -1) {
        for (SHORT nPortrait = 0; nPortrait < 6; nPortrait++) {
            if (m_characterPortraits[nPortrait] == nCharacterId) {
                return nPortrait;
            }
        }
    }

    return -1;
}

// 0x5AF510
SHORT CInfGame::GetFixedOrderCharacterPortraitNum(LONG nCharacterId)
{
    if (nCharacterId != -1) {
        for (SHORT nPortrait = 0; nPortrait < 6; nPortrait++) {
            if (m_characters[nPortrait] == nCharacterId) {
                return nPortrait;
            }
        }
    }

    return -1;
}

// 0x5AF700
void CInfGame::RemoveFamiliarResRef(const CResRef& resRef, BYTE nAlignment, BYTE nLevel)
{
    if (nAlignment < 9 && nLevel < 9) {
        POSITION pos = m_familiarResRefs[nAlignment][nLevel].GetHeadPosition();
        while (pos != NULL) {
            POSITION curr = pos;
            CResRef* pResRef = m_familiarResRefs[nAlignment][nLevel].GetNext(pos);
            if (*pResRef == resRef) {
                m_familiarResRefs[nAlignment][nLevel].RemoveAt(curr);
                break;
            }
        }
    }
}

// 0x5AF770
void CInfGame::RenderPortrait(DWORD portraitId, const CPoint& cpRenderPosition, const CSize& szControl, BOOL bPressed, BOOL reorderHighlight, BOOL selectFromMarker, const CRect& rClip, BOOL bDoubleSize)
{
    LONG iSprite = GetCharacterId(static_cast<SHORT>(portraitId));
    if (iSprite != CGameObjectArray::INVALID_INDEX) {
        CGameSprite* pSprite;

        BYTE rc = m_cObjectArray.GetShare(iSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);

        if (rc == CGameObjectArray::SUCCESS) {
            pSprite->RenderPortrait(cpRenderPosition,
                szControl,
                bPressed,
                reorderHighlight,
                selectFromMarker,
                rClip,
                bDoubleSize);

            m_cObjectArray.ReleaseShare(iSprite,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x5AF7F0
void CInfGame::WorldEngineActivated(CVidMode* pVidMode)
{
    m_nState = 0;
    m_cButtonArray.m_nSelectedButton = 100;
    m_cVRamPool.DetachSurfaces();
    m_cVRamPool.AttachSurfaces(pVidMode);
    m_gameAreas[m_visibleArea]->OnActivation();
    m_cButtonArray.ResetState();
}

// 0x5AF850
void CInfGame::WorldEngineDeactivated()
{
    if (m_gameAreas[m_visibleArea] != NULL) {
        m_gameAreas[m_visibleArea]->OnDeactivation();
    }
}

// 0x5AF870
void CInfGame::OnPortraitLClick(DWORD id)
{
    if (m_nState != 0 && GetVisibleArea()->m_iPicked != -1) {
        CGameObject* pObject;
        BYTE rc = m_cObjectArray.GetShare(GetVisibleArea()->m_iPicked,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            if (m_nState == 1) {
                GetVisibleArea()->OnActionButtonClickGround(pObject->GetPos());
            } else {
                pObject->OnActionButton(pObject->GetPos());
            }
            m_cObjectArray.ReleaseShare(GetVisibleArea()->m_iPicked,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    } else {
        if (static_cast<SHORT>(id) < GetNumCharacters()) {
            CGameSprite* pSprite;
            LONG nCharacterId = m_characterPortraits[id];
            BYTE rc = m_cObjectArray.GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
            if (rc == CGameObjectArray::SUCCESS) {
                if (!pSprite->InControl()) {
                    if (g_pBaldurChitin->GetObjectGame()->GetGroup()->GetCount() > 0
                        && pSprite->GetArea() == g_pBaldurChitin->GetObjectGame()->GetVisibleArea()) {
                        CAIAction action(CAIAction::MOVETOOBJECTFOLLOW, pSprite->GetAIType(), 0, 0, 0);
                        g_pBaldurChitin->GetObjectGame()->GetGroup()->GroupAction(action, TRUE, NULL);
                    } else if (pSprite->GetArea() != NULL) {
                        UnselectAll();

                        if (GetVisibleArea() != NULL) {
                            GetVisibleArea()->m_bPicked = FALSE;
                            GetVisibleArea()->m_iPicked = -1;
                            GetVisibleArea()->m_nToolTip = 0;
                            GetVisibleArea()->OnDeactivation();
                        }

                        m_visibleArea = pSprite->GetArea()->m_id;

                        GetVisibleArea()->OnActivation();
                    }
                }

                m_cObjectArray.ReleaseShare(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }

            if (!g_pBaldurChitin->GetScreenWorld()->GetCtrlKey()
                && !g_pBaldurChitin->GetScreenWorld()->GetShiftKey()) {
                UnselectAll();
            }

            SelectCharacter(m_characterPortraits[id], TRUE);
            SelectToolbar();
        }
    }
}

// 0x5AFCB0
void CInfGame::OnPortraitLDblClick(DWORD id)
{
    LONG iSprite = GetCharacterId(static_cast<SHORT>(id));
    if (iSprite != CGameObjectArray::INVALID_INDEX) {
        CGameSprite* pSprite;

        BYTE rc = m_cObjectArray.GetShare(iSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);

        if (rc == CGameObjectArray::SUCCESS) {
            CGameArea* pArea = pSprite->GetArea();
            if (pArea != NULL) {
                CRect rView(pArea->GetInfinity()->rViewPort);
                INT x = rView.Width() / -2;
                INT y = rView.Height() / -2;

                CPoint& pos = pSprite->GetPos();

                pArea->GetInfinity()->SetViewPosition(x + pos.x, y + pos.y, TRUE);
            }

            m_cObjectArray.ReleaseShare(iSprite,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x5AFD90
void CInfGame::EnablePortrait(BYTE nControlId, BOOL bEnable)
{
    g_pBaldurChitin->m_pEngineCharacter->EnablePortrait(1, nControlId, bEnable);
    g_pBaldurChitin->m_pEngineInventory->EnablePortrait(1, nControlId, bEnable);
    g_pBaldurChitin->m_pEngineJournal->EnablePortrait(1, nControlId, bEnable);
    g_pBaldurChitin->m_pEngineMap->EnablePortrait(1, nControlId, bEnable);
    g_pBaldurChitin->m_pEngineOptions->EnablePortrait(1, nControlId, bEnable);
    g_pBaldurChitin->m_pEngineSpellbook->EnablePortrait(1, nControlId, bEnable);
    g_pBaldurChitin->m_pEngineStore->EnablePortrait(1, nControlId, bEnable);
    g_pBaldurChitin->m_pEngineWorld->EnablePortrait(1, nControlId, bEnable);
}

// 0x5AFE60
BOOLEAN CInfGame::ExtendedDayNightAreaActive()
{
    for (int index = 0; index < CINFGAME_MAX_AREAS; index++) {
        if ((m_gameAreas[index]->m_header.m_areaType & 0x40) != 0) {
            return TRUE;
        }
    }

    return FALSE;
}

// 0x5AFE90
void CInfGame::UpdatePortraitToolTip(DWORD dwControlId, STRREF dwStrId)
{
    CUIManager* pManager;
    CUIPanel* pPanel;
    CUIControlBase* pControl;

    pManager = g_pBaldurChitin->m_pEngineCharacter->GetManager();
    pPanel = pManager->GetPanel(1);
    pControl = pPanel->GetControl(dwControlId);
    pControl->SetToolTipStrRef(dwStrId, -1, -1);

    pManager = g_pBaldurChitin->m_pEngineInventory->GetManager();
    pPanel = pManager->GetPanel(1);
    pControl = pPanel->GetControl(dwControlId);
    pControl->SetToolTipStrRef(dwStrId, -1, -1);

    pManager = g_pBaldurChitin->m_pEngineJournal->GetManager();
    pPanel = pManager->GetPanel(1);
    pControl = pPanel->GetControl(dwControlId);
    pControl->SetToolTipStrRef(dwStrId, -1, -1);

    pManager = g_pBaldurChitin->m_pEngineMap->GetManager();
    pPanel = pManager->GetPanel(1);
    pControl = pPanel->GetControl(dwControlId);
    pControl->SetToolTipStrRef(dwStrId, -1, -1);

    pManager = g_pBaldurChitin->m_pEngineOptions->GetManager();
    pPanel = pManager->GetPanel(1);
    pControl = pPanel->GetControl(dwControlId);
    pControl->SetToolTipStrRef(dwStrId, -1, -1);

    pManager = g_pBaldurChitin->m_pEngineSpellbook->GetManager();
    pPanel = pManager->GetPanel(1);
    pControl = pPanel->GetControl(dwControlId);
    pControl->SetToolTipStrRef(dwStrId, -1, -1);

    pManager = g_pBaldurChitin->m_pEngineStore->GetManager();
    pPanel = pManager->GetPanel(1);
    pControl = pPanel->GetControl(dwControlId);
    pControl->SetToolTipStrRef(dwStrId, -1, -1);

    pManager = g_pBaldurChitin->m_pEngineWorld->GetManager();
    pPanel = pManager->GetPanel(1);
    pControl = pPanel->GetControl(dwControlId);
    pControl->SetToolTipStrRef(dwStrId, -1, -1);
}

// 0x5B0020
void CInfGame::StepAnimation(BYTE direction)
{
    // FIXME: Should be static global.
    WORD animationList[ANIM_TYPE_NUM] = {
        ANIM_DEATH_KNIGHT,
        ANIM_STATIC_NOBLE_MAN_CHAIR,
        ANIM_STATIC_NOBLE_MAN_MATTE,
        ANIM_STATIC_NOBLE_WOMAN_CHAIR,
        ANIM_STATIC_NOBLE_WOMAN_MATTE,
        ANIM_STATIC_PEASANT_MAN_CHAIR,
        ANIM_STATIC_PEASANT_MAN_STOOL,
        ANIM_STATIC_PEASANT_MAN_MATTE,
        ANIM_STATIC_PEASANT_WOMAN_CHAIR,
        ANIM_STATIC_PEASANT_WOMAN_MATTE,
        ANIM_STATIC_HUMANCLERIC_MAN_CHAIR,
        ANIM_SLEEPING_MAN_HUMAN,
        ANIM_SLEEPING_WOMAN_HUMAN,
        ANIM_SLEEPING_FAT_MAN_HUMAN,
        ANIM_SLEEPING_DWARF,
        ANIM_SLEEPING_MAN_ELF,
        ANIM_SLEEPING_WOMAN_ELF,
        ANIM_SLEEPING_MAN_HALFLING,
        ANIM_CLERIC_MALE_HUMAN,
        ANIM_CLERIC_MALE_ELF,
        ANIM_CLERIC_MALE_DWARF,
        ANIM_CLERIC_MALE_HALFLING,
        ANIM_CLERIC_MALE_GNOME,
        ANIM_CLERIC_MALE_HALFORC,
        ANIM_CLERIC_FEMALE_HUMAN,
        ANIM_CLERIC_FEMALE_ELF,
        ANIM_CLERIC_FEMALE_DWARF,
        ANIM_CLERIC_FEMALE_HALFLING,
        ANIM_CLERIC_FEMALE_GNOME,
        ANIM_CLERIC_FEMALE_HALFORC,
        ANIM_FIGHTER_MALE_HUMAN,
        ANIM_FIGHTER_MALE_ELF,
        ANIM_FIGHTER_MALE_DWARF,
        ANIM_FIGHTER_MALE_HALFLING,
        ANIM_FIGHTER_MALE_GNOME,
        ANIM_FIGHTER_MALE_HALFORC,
        ANIM_FIGHTER_FEMALE_HUMAN,
        ANIM_FIGHTER_FEMALE_ELF,
        ANIM_FIGHTER_FEMALE_DWARF,
        ANIM_FIGHTER_FEMALE_HALFLING,
        ANIM_FIGHTER_FEMALE_GNOME,
        ANIM_FIGHTER_FEMALE_HALFORC,
        ANIM_MAGE_MALE_HUMAN,
        ANIM_MAGE_MALE_ELF,
        ANIM_MAGE_MALE_DWARF,
        ANIM_MAGE_MALE_GNOME,
        ANIM_MAGE_MALE_HALFORC,
        ANIM_MAGE_FEMALE_HUMAN,
        ANIM_MAGE_FEMALE_ELF,
        ANIM_MAGE_FEMALE_DWARF,
        ANIM_MAGE_FEMALE_GNOME,
        ANIM_MAGE_FEMALE_HALFORC,
        ANIM_THIEF_MALE_HUMAN,
        ANIM_THIEF_MALE_ELF,
        ANIM_THIEF_MALE_DWARF,
        ANIM_THIEF_MALE_HALFLING,
        ANIM_THIEF_MALE_GNOME,
        ANIM_THIEF_MALE_HALFORC,
        ANIM_THIEF_FEMALE_HUMAN,
        ANIM_THIEF_FEMALE_ELF,
        ANIM_THIEF_FEMALE_DWARF,
        ANIM_THIEF_FEMALE_HALFLING,
        ANIM_THIEF_FEMALE_GNOME,
        ANIM_THIEF_FEMALE_HALFORC,
        ANIM_MONK_MALE_HUMAN,
        ANIM_MONK_FEMALE_HUMAN,
        ANIM_MONK,
        ANIM_SKELETON,
        ANIM_DOOM_GUARD,
        ANIM_BEAR_BLACK,
        ANIM_BEAR_BROWN,
        ANIM_BEAR_CAVE,
        ANIM_BEAR_POLAR,
        ANIM_SPIDER_GIANT,
        ANIM_SPIDER_HUGE,
        ANIM_SPIDER_PHASE,
        ANIM_SPIDER_SWORD,
        ANIM_SPIDER_WRAITH,
        ANIM_WOLF,
        ANIM_WOLF_WINTER,
        ANIM_WEREWOLF,
        ANIM_WEREWOLF_GREATER,
        ANIM_OGRE,
        ANIM_CARRION_CRAWLER,
        ANIM_COW,
        ANIM_STATIC_BEGGAR_MAN,
        ANIM_STATIC_HARLOT_WOMAN,
        ANIM_STATIC_BOY,
        ANIM_STATIC_GIRL,
        ANIM_STATIC_FAT_MAN,
        ANIM_STATIC_FAT_WOMAN,
        ANIM_STATIC_PEASANT_MAN,
        ANIM_STATIC_PEASANT_WOMAN,
        ANIM_STATIC_NOBLE_MAN,
        ANIM_STATIC_NOBLE_WOMAN,
        ANIM_STATIC_SLAVE,
        ANIM_BAT_INSIDE,
        ANIM_CAT,
        ANIM_CHICKEN,
        ANIM_RAT,
        ANIM_SQUIRREL,
        ANIM_BAT_OUTSIDE,
        ANIM_BEGGAR_MAN,
        ANIM_HARLOT_WOMAN,
        ANIM_BOY,
        ANIM_GIRL,
        ANIM_FAT_MAN,
        ANIM_FAT_WOMAN,
        ANIM_PEASANT_MAN,
        ANIM_PEASANT_WOMAN,
        ANIM_NOBLE_MAN,
        ANIM_NOBLE_WOMAN,
        ANIM_SLAVE,
        ANIM_EAGLE,
        ANIM_VULTURE,
        ANIM_BIRD,
        ANIM_BIRD_INSIDE,
        ANIM_BEETLE,
        ANIM_BEETLE,
        ANIM_BEETLE_BOMBADIER,
        ANIM_BEETLE_BORING,
        ANIM_BEETLE_FIRE,
        ANIM_BEETLE_RHINOCEROUS,
        ANIM_CYCLOPS,
        ANIM_ETTIN,
        ANIM_GIANT_FIRE,
        ANIM_GIANT_FROST,
        ANIM_VERBEEG,
        ANIM_ELEMENTAL_EARTH,
        ANIM_ELEMENTAL_FIRE,
        ANIM_ELEMENTAL_WATER,
        ANIM_GHOUL,
        ANIM_GHOUL_2,
        ANIM_WIGHT,
        ANIM_ZOMBIE,
        ANIM_ZOMBIE_2,
        ANIM_WIGHT_2,
        ANIM_WIGHT_3,
        ANIM_MUMMY,
        ANIM_MUMMY_HISTACHII,
        ANIM_GOBLIN_W_AXE,
        ANIM_GOBLIN_W_BOW,
        ANIM_GOBLIN_ELITE_W_AXE,
        ANIM_GOBLIN_ELITE_W_BOW,
        ANIM_SVIRFNEBLIN,
        ANIM_GOLEM_IRON,
        ANIM_SKELETON_BONEGUARD,
        ANIM_MINOTAUR,
        ANIM_TROLL,
        ANIM_TROLL_ICE,
        ANIM_TROLL_SNOW,
        ANIM_UMBER_HULK,
        ANIM_TUNDRA_YETI,
        ANIM_ORC_W_AXE,
        ANIM_ORC_W_BOW,
        ANIM_ORC_ELITE_W_AXE,
        ANIM_ORC_ELITE_W_BOW,
        ANIM_ORC_SHAMAN,
        ANIM_OROG,
        ANIM_OROG_ELITE,
        ANIM_OROG_CHIEFTAN,
        ANIM_LIZARD_MAN_KING,
        ANIM_YUAN_TI_PRIEST,
        ANIM_SHADOW,
        ANIM_GHOST,
        ANIM_ANIMATED_PLATE,
        ANIM_ANIMATED_PLATE_2,
        ANIM_ANIMATED_PLATE_3,
        ANIM_BELHIFET,
        ANIM_BELHIFET_PRIEST,
        ANIM_LICH_LARREL,
        ANIM_MYCONID_RED,
        ANIM_MYCONID_BLUE,
        ANIM_SHRIEKER,
        ANIM_TANARI,
        ANIM_SALAMANDER,
        ANIM_SALAMANDER_FROST,
        ANIM_SKELETON_ARMORED,
        ANIM_YUAN_TI,
        ANIM_YUAN_TI_ELITE,
        ANIM_SKELETON_TATTERED,
        ANIM_LIZARD_MAN_2H,
        ANIM_LIZARD_MAN_1H,
        ANIM_ARUNDEL,
        ANIM_CREEPING_DOOM,
        ANIM_LICH_WHITE,
        ANIM_SVIRVNEBLIN_NOAXE,
        ANIM_BARBARIAN_1,
        ANIM_BARBARIAN_2,
        ANIM_BARBARIAN_3,
        ANIM_BARBARIAN_SHAMAN_1,
        ANIM_BARBARIAN_SHAMAN_2,
        ANIM_BARBARIAN_SHAMAN_3,
        ANIM_BARBARIAN_ICASARACHT,
        ANIM_CORNUGON,
        ANIM_DRAGON_WHITE,
        ANIM_DROWNED_DEAD,
        ANIM_GLABREZU,
        ANIM_GOLEM_ICE,
        ANIM_REMORHAZ,
        ANIM_SAHUAGIN,
        ANIM_SAHUAGIN_LARGE,
        ANIM_SEER,
        ANIM_ELEMENTAL_SHAMBLING_MOUND,
        ANIM_TROLL_SCRAG,
        ANIM_UMBER_HULK_VODYANOI,
        ANIM_WAILING_VIRGIN,
        ANIM_WATER_WEIRD,
        ANIM_WIGHT_BARROW,
        ANIM_WYVERN,
        ANIM_WYVERN_BIG_WHITE,
        ANIM_DOG_WILD,
        ANIM_DOG_WAR,
        ANIM_DOG_MOON,
        ANIM_HARPY,
        ANIM_BEHOLDER,
        ANIM_RAKSHASA,
        ANIM_SLIME_GREEN,
        ANIM_SLIME_OLIVE,
        ANIM_SLIME_MUSTARD,
        ANIM_SLIME_OCHRE,
        ANIM_DEER,
        ANIM_DJINNI,
        ANIM_DJINNI_LEGS,
        ANIM_DOPPLEGANGER,
        ANIM_DOPPLEGANGER_GREATER,
        ANIM_DRAGON_BLACK,
        ANIM_ELEMENTAL_AIR,
        ANIM_ELEMENTAL_AIR_SMALL,
        ANIM_ELEMENTAL_FIRE_BG2,
        ANIM_ETTERCAP,
        ANIM_GNOLL,
        ANIM_GOLEM_CLAY,
        ANIM_GOLEM_IRON_BG2,
        ANIM_GREATCAT_PANTHER,
        ANIM_GREATCAT_LEOPARD,
        ANIM_HOBGOBLIN,
        ANIM_IMP,
        ANIM_MINDFLAYER,
        ANIM_OGRE_HALF,
        ANIM_OTYUGH,
        ANIM_RAVER,
        ANIM_SLAYER,
        ANIM_SNAKE,
        ANIM_SPIDER_SMALL,
        ANIM_ABISHAI_WHITE,
        ANIM_BOAR_ARCTIC,
        ANIM_BUGBEAR,
        ANIM_BUGBEAR_CAPTAIN,
        ANIM_CHIMERA,
        ANIM_DARKTREE,
        ANIM_DRAGON_WHITE_BABY,
        ANIM_DRIDER_MALE,
        ANIM_DRIDER_FEMALE,
        ANIM_DROW,
        ANIM_DROW_FEMALE,
        ANIM_ELEMENTAL_WATER_SMALL,
        ANIM_FEYR,
        ANIM_FEYR_GREATER,
        ANIM_GIANT_FOMORIAN,
        ANIM_GOBLIN_CAPTAIN,
        ANIM_GOBLIN_SHAMAN,
        ANIM_GOBLIN_WORG,
        ANIM_GOBLIN_WORG_CAPTAIN,
        ANIM_HALFDRAGON_BLACK,
        ANIM_HALFDRAGON_RED,
        ANIM_HOOK_HORROR,
        ANIM_INVISIBLE_TINY,
        ANIM_INVISIBLE_SMALL,
        ANIM_INVISIBLE_MEDIUM,
        ANIM_INVISIBLE_LARGE,
        ANIM_INVISIBLE_GIANT,
        ANIM_INVISIBLE_HUGE,
        ANIM_ISAIR,
        ANIM_KEG_1,
        ANIM_KEG_2,
        ANIM_KEG_3,
        ANIM_LEMURE,
        ANIM_MADAE,
        ANIM_MALARITE,
        ANIM_ORC_W_AXE_UNDEAD,
        ANIM_ORC_SHAMAN_UNDEAD,
        ANIM_OROG_SHAMAN,
        ANIM_SHENRICAL,
        ANIM_WEREBADGER,
        ANIM_WERERAT,
        ANIM_WORG,
        ANIM_WILL_O_WISP,
        ANIM_YUAN_TI_HALF,
    };

    // FIXME: Should be static global.
    CString animationListNames[ANIM_TYPE_NUM] = {
        "DEATH_KNIGHT",
        "STATIC_NOBLE_MAN_CHAIR",
        "STATIC_NOBLE_MAN_MATTE",
        "STATIC_NOBLE_WOMAN_CHAIR",
        "STATIC_NOBLE_WOMAN_MATTE",
        "STATIC_PEASANT_MAN_CHAIR",
        "STATIC_PEASANT_MAN_STOOL",
        "STATIC_PEASANT_MAN_MATTE",
        "STATIC_PEASANT_WOMAN_CHAIR",
        "STATIC_PEASANT_WOMAN_MATTE",
        "STATIC_HUMANCLERIC_MAN_CHAIR",
        "SLEEPING_MAN_HUMAN",
        "SLEEPING_WOMAN_HUMAN",
        "SLEEPING_FAT_MAN_HUMAN",
        "SLEEPING_DWARF",
        "SLEEPING_MAN_ELF",
        "SLEEPING_WOMAN_ELF",
        "SLEEPING_MAN_HALFLING",
        "CLERIC_MALE_HUMAN",
        "CLERIC_MALE_ELF",
        "CLERIC_MALE_DWARF",
        "CLERIC_MALE_HALFLING",
        "CLERIC_MALE_GNOME",
        "CLERIC MALE HALFORC",
        "CLERIC_FEMALE_HUMAN",
        "CLERIC_FEMALE_ELF",
        "CLERIC_FEMALE_DWARF",
        "CLERIC_FEMALE_HALFLING",
        "CLERIC_FEMALE_GNOME",
        "CLERIC FEMALE HALFORC",
        "FIGHTER_MALE_HUMAN",
        "FIGHTER_MALE_ELF",
        "FIGHTER_MALE_DWARF",
        "FIGHTER_MALE_HALFLING",
        "FIGHTER_MALE_GNOME",
        "FIGHTER MALE HALFORC",
        "FIGHTER_FEMALE_HUMAN",
        "FIGHTER_FEMALE_ELF",
        "FIGHTER_FEMALE_DWARF",
        "FIGHTER_FEMALE_HALFLING",
        "FIGHTER_FEMALE_GNOME",
        "FIGHTER FEMALE HALFORC",
        "MAGE_MALE_HUMAN",
        "MAGE_MALE_ELF",
        "MAGE_MALE_DWARF",
        "MAGE_MALE_GNOME",
        "MAGE MALE HALFORC",
        "MAGE_FEMALE_HUMAN",
        "MAGE_FEMALE_ELF",
        "MAGE_FEMALE_DWARF",
        "MAGE_FEMALE_GNOME",
        "MAGE FEMALE HALFORC",
        "THIEF_MALE_HUMAN",
        "THIEF_MALE_ELF",
        "THIEF_MALE_DWARF",
        "THIEF_MALE_HALFLING",
        "THIEF_MALE_GNOME",
        "THIEF MALE HALFORC",
        "THIEF_FEMALE_HUMAN",
        "THIEF_FEMALE_ELF",
        "THIEF_FEMALE_DWARF",
        "THIEF_FEMALE_HALFLING",
        "THIEF_FEMALE_GNOME",
        "THIEF FEMALE HALFORC",
        "MONK MALE HUMAN",
        "MONK FEMALE HUMAN",
        "MONK",
        "SKELETON",
        "DOOM_GUARD",
        "BEAR_BLACK",
        "BEAR_BROWN",
        "BEAR_CAVE",
        "BEAR_POLAR",
        "SPIDER_GIANT",
        "SPIDER_HUGE",
        "SPIDER_PHASE",
        "SPIDER_SWORD",
        "SPIDER_WRAITH",
        "WOLF",
        "WOLF_WINTER",
        "WEREWOLF",
        "WEREWOLF_GREATER",
        "OGRE",
        "CARRION_CRAWLER",
        "COW",
        "STATIC_BEGGAR_MAN",
        "STATIC_HARLOT_WOMAN",
        "STATIC_BOY",
        "STATIC_GIRL",
        "STATIC_FAT_MAN",
        "STATIC_FAT_WOMAN",
        "STATIC_PEASANT_MAN",
        "STATIC_PEASANT_WOMAN",
        "STATIC_NOBLE_MAN",
        "STATIC_NOBLE_WOMAN",
        "STATIC_SLAVE",
        "BAT_INSIDE",
        "CAT",
        "CHICKEN",
        "RAT",
        "SQUIRREL",
        "BAT_OUTSIDE",
        "BEGGAR_MAN",
        "HARLOT_WOMAN",
        "BOY",
        "GIRL",
        "FAT_MAN",
        "FAT_WOMAN",
        "PEASANT_MAN",
        "PEASANT_WOMAN",
        "NOBLE_MAN",
        "NOBLE_WOMAN",
        "SLAVE",
        "EAGLE",
        "VULTURE",
        "BIRD",
        "BIRD_INSIDE",
        "BEETLE",
        "BEETLE",
        "BEETLE_BOMBADIER",
        "BEETLE_BORING",
        "BEETLE_FIRE",
        "BEETLE_RHINOCEROUS",
        "CYCLOPS",
        "ETTIN",
        "GIANT_FIRE",
        "GIANT_FROST",
        "VERBEEG",
        "ELEMENTAL_EARTH",
        "ELEMENTAL_FIRE",
        "ELEMENTAL_WATER",
        "GHOUL",
        "GHOUL_2",
        "WIGHT",
        "ZOMBIE",
        "ZOMBIE_2",
        "WIGHT_2",
        "WIGHT_3",
        "MUMMY",
        "MUMMY_HISTACHII",
        "GOBLIN_W_AXE",
        "GOBLIN_W_BOW",
        "GOBLIN_ELITE_W_AXE",
        "GOBLIN_ELITE_W_BOW",
        "SVIRFNEBLIN",
        "GOLEM_IRON",
        "SKELETON_BONEGUARD",
        "MINOTAUR",
        "TROLL",
        "TROLL_ICE",
        "TROLL_SNOW",
        "UMBER_HULK",
        "TUNDRA_YETI",
        "ORC_W_AXE",
        "ORC_W_BOW",
        "ORC_ELITE_W_AXE",
        "ORC_ELITE_W_BOW",
        "ORC_SHAMAN",
        "OROG",
        "OROG_ELITE",
        "OROG_CHIEFTAN",
        "LIZARD_MAN_KING",
        "YUAN_TI_PRIEST",
        "SHADOW",
        "GHOST",
        "ANIMATED_PLATE",
        "ANIMATED_PLATE_2",
        "ANIMATED_PLATE_3",
        "BELHIFET",
        "BELHIFET_PRIEST",
        "LICH_LARREL",
        "MYCONID_RED",
        "MYCONID_BLUE",
        "SHRIEKER",
        "TANARI",
        "SALAMANDER",
        "SALAMANDER_FROST",
        "SKELETON_ARMORED",
        "YUAN_TI",
        "YUAN_TI_ELITE",
        "SKELETON_TATTERED",
        "LIZARD_MAN_2H",
        "LIZARD_MAN_1H",
        "ARUNDEL",
        "CREEPING_DOOM",
        "LICH_WHITE",
        "SVIRVNEBLIN_NOAXE",
        "BARBARIAN_1",
        "BARBARIAN_2",
        "BARBARIAN_3",
        "BARBARIAN_SHAMAN_1",
        "BARBARIAN_SHAMAN_2",
        "BARBARIAN_SHAMAN_3",
        "BARBARIAN_ICASARACHT",
        "CORNUGON",
        "DRAGON_WHITE",
        "DROWNED_DEAD",
        "GLABREZU",
        "GOLEM_ICE",
        "REMORHAZ",
        "SAHUAGIN",
        "SAHUAGIN_LARGE",
        "SEER",
        "ELEMENTAL_SHAMBLING_MOUND",
        "TROLL_SCRAG",
        "UMBER_HULK_VODYANOI",
        "WAILING_VIRGIN",
        "WATER_WEIRD",
        "WIGHT_BARROW",
        "WYVERN",
        "WYVERN_BIG_WHITE",
        "DOG_WILD",
        "DOG_WAR",
        "DOG_MOON",
        "HARPY",
        "BEHOLDER",
        "RAKSHASA",
        "SLIME_GREEN",
        "SLIME_OLIVE",
        "SLIME_MUSTARD",
        "SLIME_OCHRE",
        "DEER",
        "DJINNI",
        "DJINNI_LEGS",
        "DOPPLEGANGER",
        "DOPPLEGANGER_GREATER",
        "DRAGON_BLACK",
        "ELEMENTAL_AIR",
        "ELEMENTAL_AIR_SMALL",
        "ELEMENTAL_FIRE_BG2",
        "ETTERCAP",
        "GNOLL",
        "GOLEM_CLAY",
        "GOLEM_IRON_BG2",
        "GREATCAT_PANTHER",
        "GREATCAT_LEOPARD",
        "HOBGOBLIN",
        "IMP",
        "MINDFLAYER",
        "OGRE_HALF",
        "OTYUGH",
        "RAVER",
        "SLAYER",
        "SNAKE",
        "SPIDER_SMALL",
        "ABISHAI_WHITE",
        "BOAR_ARCTIC",
        "BUGBEAR",
        "BUGBEAR_CAPTAIN",
        "CHIMERA",
        "DARKTREE",
        "DRAGON_WHITE_BABY",
        "DRIDER_MALE",
        "DRIDER_FEMALE",
        "DROW",
        "DROW_FEMALE",
        "ELEMENTAL_WATER_SMALL",
        "FEYR",
        "FEYR_GREATER",
        "GIANT_FOMORIAN",
        "GOBLIN_CAPTAIN",
        "GOBLIN_SHAMAN",
        "GOBLIN_WORG",
        "GOBLIN_WORG_CAPTAIN",
        "HALFDRAGON_BLACK",
        "HALFDRAGON_RED",
        "HOOK_HORROR",
        "INVISIBLE_TINY",
        "INVISIBLE_SMALL",
        "INVISIBLE_MEDIUM",
        "INVISIBLE_LARGE",
        "INVISIBLE_GIANT",
        "INVISIBLE_HUGE",
        "ISAIR",
        "KEG_1",
        "KEG_2",
        "KEG_3",
        "LEMURE",
        "MADAE",
        "MALARITE",
        "ORC_W_AXE_UNDEAD",
        "ORC_SHAMAN_UNDEAD",
        "OROG_SHAMAN",
        "SHENRICAL",
        "WEREBADGER",
        "WERERAT",
        "WORG",
        "WILL_O_WISP",
        "YUAN_TI_HALF",
    };

    if (!m_group.m_memberList.IsEmpty()) {
        CRect rFx;
        CPoint ptRef;

        LONG nCharacterId = m_group.GetGroupLeader();

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = m_cObjectArray.GetDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            switch (direction) {
            case '1':
                m_currArmor++;
                if (m_currArmor > '4') {
                    m_currArmor = '1';
                }

                pSprite->GetAnimation()->EquipArmor(static_cast<CHAR>(m_currArmor),
                    pSprite->GetBaseStats()->m_colors);

                g_pBaldurChitin->m_pEngineWorld->DisplayText(CString(""),
                    CString("Armor Level ") + static_cast<CHAR>(m_currArmor),
                    -1,
                    0);
                break;
            case '4':
                for (int index = 0; index < ANIM_TYPE_NUM; index++) {
                    pSprite->UnequipAll(FALSE);
                    pSprite->GetAnimation()->SetAnimationType(animationList[index],
                        pSprite->GetBaseStats()->m_colors,
                        0);
                    pSprite->EquipAll(FALSE);

                    for (SHORT nSequence = 0; nSequence <= 15; nSequence++) {
                        if (nSequence != 8) {
                            pSprite->GetAnimation()->SetSequence(nSequence);
                            pSprite->SetSequence(nSequence);

                            for (SHORT nDirection = 0; nDirection <= 15; nDirection++) {
                                pSprite->GetAnimation()->ChangeDirection(nDirection);

                                for (INT nFrame = 0; nFrame < 40; nFrame--) {
                                    pSprite->GetAnimation()->CalculateFxRect(rFx, ptRef, pSprite->m_posZ);
                                    pSprite->GetAnimation()->IncrementFrame();
                                }
                            }
                        }
                    }
                }
                break;
            case '5':
                for (int index = 0; index < ANIM_TYPE_NUM; index++) {
                    pSprite->UnequipAll(FALSE);
                    pSprite->GetAnimation()->SetAnimationType(animationList[index],
                        pSprite->GetBaseStats()->m_colors,
                        0);
                    pSprite->EquipAll(FALSE);

                    for (SHORT nSequence = 0; nSequence <= 15; nSequence++) {
                        if (nSequence == 8) {
                            pSprite->GetAnimation()->SetSequence(nSequence);
                            pSprite->SetSequence(nSequence);

                            for (SHORT nDirection = 0; nDirection <= 15; nDirection++) {
                                pSprite->GetAnimation()->ChangeDirection(nDirection);

                                for (INT nFrame = 0; nFrame < 40; nFrame--) {
                                    pSprite->GetAnimation()->CalculateFxRect(rFx, ptRef, pSprite->m_posZ);
                                    pSprite->GetAnimation()->IncrementFrame();
                                }
                            }
                        }
                    }
                }
                break;
            case '6':
                if (pSprite->GetVertListType() != CGameObject::LIST_FLIGHT) {
                    pSprite->GetArea()->m_search.RemoveObject(CPoint(pSprite->GetPos().x / CPathSearch::GRID_SQUARE_SIZEX,
                                                                  pSprite->GetPos().y / CPathSearch::GRID_SQUARE_SIZEY),
                        pSprite->GetAIType().m_nEnemyAlly,
                        pSprite->GetAnimation()->GetPersonalSpace(),
                        pSprite->field_54A8,
                        pSprite->field_7430);
                }

                m_currAnimation--;
                if (m_currAnimation < 0) {
                    m_currAnimation = ANIM_TYPE_NUM - 1;
                }

                pSprite->UnequipAll(FALSE);
                pSprite->GetAnimation()->SetAnimationType(animationList[m_currAnimation],
                    pSprite->GetBaseStats()->m_colors,
                    0);
                pSprite->EquipAll(FALSE);

                pSprite->GetAnimation()->SetSequence(pSprite->m_nSequence);

                g_pBaldurChitin->m_pEngineWorld->DisplayText(CString(""),
                    animationListNames[m_currAnimation],
                    -1,
                    FALSE);

                if (pSprite->GetVertListType() != CGameObject::LIST_FLIGHT) {
                    pSprite->GetArea()->m_search.AddObject(CPoint(pSprite->GetPos().x / CPathSearch::GRID_SQUARE_SIZEX,
                                                               pSprite->GetPos().y / CPathSearch::GRID_SQUARE_SIZEY),
                        pSprite->GetAIType().m_nEnemyAlly,
                        pSprite->GetAnimation()->GetPersonalSpace(),
                        pSprite->field_54A8,
                        pSprite->field_7430);
                }
                break;
            case '7':
                if (pSprite->GetVertListType() != CGameObject::LIST_FLIGHT) {
                    pSprite->GetArea()->m_search.RemoveObject(CPoint(pSprite->GetPos().x / CPathSearch::GRID_SQUARE_SIZEX,
                                                                  pSprite->GetPos().y / CPathSearch::GRID_SQUARE_SIZEY),
                        pSprite->GetAIType().m_nEnemyAlly,
                        pSprite->GetAnimation()->GetPersonalSpace(),
                        pSprite->field_54A8,
                        pSprite->field_7430);
                }

                m_currAnimation++;
                if (m_currAnimation == ANIM_TYPE_NUM) {
                    m_currAnimation = 0;
                }

                pSprite->UnequipAll(FALSE);
                pSprite->GetAnimation()->SetAnimationType(animationList[m_currAnimation],
                    pSprite->GetBaseStats()->m_colors,
                    0);
                pSprite->EquipAll(FALSE);

                pSprite->GetAnimation()->SetSequence(pSprite->m_nSequence);

                g_pBaldurChitin->m_pEngineWorld->DisplayText(CString(""),
                    animationListNames[m_currAnimation],
                    -1,
                    FALSE);

                if (pSprite->GetVertListType() != CGameObject::LIST_FLIGHT) {
                    pSprite->GetArea()->m_search.AddObject(CPoint(pSprite->GetPos().x / CPathSearch::GRID_SQUARE_SIZEX,
                                                               pSprite->GetPos().y / CPathSearch::GRID_SQUARE_SIZEY),
                        pSprite->GetAIType().m_nEnemyAlly,
                        pSprite->GetAnimation()->GetPersonalSpace(),
                        pSprite->field_54A8,
                        pSprite->field_7430);
                }
                break;
            }

            m_cObjectArray.ReleaseDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// FIXME: Use reference instead of object instance.
//
// 0x5B7510
CGameArea* CInfGame::GetArea(CString sName)
{
    for (int index = 0; index < CINFGAME_MAX_AREAS; index++) {
        if (m_gameAreas[index] != NULL) {
            if (m_gameAreas[index]->m_resRef == sName) {
                return m_gameAreas[index];
            }
        }
    }

    return NULL;
}

// 0x68DE70
CGameArea* CInfGame::GetArea(SHORT nArea)
{
    // __FILE__: .\Include\InfGame.h
    // __LINE__: 1416
    UTIL_ASSERT(nArea < CINFGAME_MAX_AREAS);

    return m_gameAreas[nArea];
}

// 0x5B75C0
LONG CInfGame::GetGroundPile(LONG iSprite)
{
    CGameSprite* pSprite;

    BYTE rc = m_cObjectArray.GetShare(iSprite,
        CGameObjectArray::THREAD_ASYNCH,
        reinterpret_cast<CGameObject**>(&pSprite),
        INFINITE);

    if (rc != CGameObjectArray::SUCCESS) {
        return CGameObjectArray::INVALID_INDEX;
    }

    CPoint ptSprite = pSprite->GetPos();
    LONG iGroundPile = pSprite->GetArea()->GetGroundPile(ptSprite);
    if (iGroundPile == CGameObjectArray::INVALID_INDEX) {
        ptSprite.x += CPathSearch::GRID_SQUARE_SIZEX / 2
            - ptSprite.x % CPathSearch::GRID_SQUARE_SIZEX;
        ptSprite.y += CPathSearch::GRID_SQUARE_SIZEY / 2
            - ptSprite.y % CPathSearch::GRID_SQUARE_SIZEY;

        CGameContainer* pContainer = new CGameContainer(pSprite->GetArea(),
            CRect(ptSprite.x - CPathSearch::GRID_SQUARE_SIZEX / 2,
                ptSprite.y - CPathSearch::GRID_SQUARE_SIZEY / 2,
                ptSprite.x + CPathSearch::GRID_SQUARE_SIZEX / 2 + 1,
                ptSprite.y + CPathSearch::GRID_SQUARE_SIZEY / 2 + 1));
        iGroundPile = pContainer->GetId();
    }

    m_cObjectArray.ReleaseShare(iSprite,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return iGroundPile;
}

// 0x5B7FF0
SHORT CInfGame::GetNumGroundSlots(LONG nContainerId)
{
    SHORT nSlots;

    CGameContainer* pContainer;

    BYTE rc = m_cObjectArray.GetShare(nContainerId,
        CGameObjectArray::THREAD_ASYNCH,
        reinterpret_cast<CGameObject**>(&pContainer),
        INFINITE);

    if (rc == CGameObjectArray::SUCCESS) {
        nSlots = static_cast<SHORT>(pContainer->m_lstItems.GetCount());

        m_cObjectArray.ReleaseShare(nContainerId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    } else {
        nSlots = 0;
    }

    return nSlots;
}

// 0x5B8040
void CInfGame::InventoryInfoGround(LONG nContainerId, SHORT nSlotNum, CItem*& pItem, STRREF& description, CResRef& cResIcon, CResRef& cResItem, WORD& wCount)
{
    description = -1;
    cResIcon = "";
    cResItem = "";
    wCount = 0;

    CGameContainer* pContainer;

    BYTE rc = m_cObjectArray.GetShare(nContainerId,
        CGameObjectArray::THREAD_ASYNCH,
        reinterpret_cast<CGameObject**>(&pContainer),
        INFINITE);

    if (rc == CGameObjectArray::SUCCESS) {
        pItem = pContainer->GetItem(nSlotNum);
        if (pItem != NULL) {
            description = pItem->GetGenericName();
            cResIcon = pItem->GetItemIcon();
            cResItem = pItem->GetResRef();

            if (pItem->GetMaxStackable() > 1) {
                wCount = pItem->GetUsageCount(0);
            }
        }

        m_cObjectArray.ReleaseShare(nContainerId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x5B8130
WORD CInfGame::GetContainerType(LONG nContainerId)
{
    CGameContainer* pContainer;

    BYTE rc = m_cObjectArray.GetShare(nContainerId,
        CGameObjectArray::THREAD_ASYNCH,
        reinterpret_cast<CGameObject**>(&pContainer),
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return -1;
    }

    WORD nContainerType = pContainer->m_containerType;

    m_cObjectArray.ReleaseShare(nContainerId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return nContainerType;
}

// 0x5B8180
void CInfGame::CompressContainer(LONG nContainerId)
{
    CGameContainer* pContainer;

    BYTE rc = m_cObjectArray.GetDeny(nContainerId,
        CGameObjectArray::THREAD_ASYNCH,
        reinterpret_cast<CGameObject**>(&pContainer),
        INFINITE);
    if (rc = CGameObjectArray::SUCCESS) {
        pContainer->CompressContainer();

        m_cObjectArray.ReleaseDeny(nContainerId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x5BA080
INT CInfGame::CheckItemUsable(CGameSprite* pSprite, CItem* pItem)
{
    INT nType = pItem->GetItemType();
    if (nType != 35 && nType != 11) {
        return 1;
    }

    DWORD nNotUsable1 = pItem->GetNotUsableBy();
    DWORD nNotUsable2 = pItem->GetNotUsableBy2();
    DWORD nClassMask = pSprite->GetDerivedStats()->m_classMask;
    DWORD nSpecialization = pSprite->GetDerivedStats()->m_nSpecialization;

    if ((nNotUsable1 & 0x1) == 0
        && (nClassMask & CLASSMASK_BARBARIAN) != 0) {
        return 1;
    }

    if ((nNotUsable1 & 0x2) == 0
        && (nClassMask & CLASSMASK_BARD) != 0) {
        return 1;
    }

    if ((nNotUsable1 & 0x4) == 0
        && (nClassMask & CLASSMASK_CLERIC) != 0
        && ((nNotUsable2 & nSpecialization) & SPECMASK_CLERIC) == 0) {
        return 1;
    }

    if ((nNotUsable1 & 0x8) == 0
        && (nClassMask & CLASSMASK_DRUID) != 0) {
        return 1;
    }

    if ((nNotUsable1 & 0x10) == 0
        && (nClassMask & CLASSMASK_FIGHTER) != 0) {
        return 1;
    }

    if ((nNotUsable1 & 0x20) == 0
        && (nClassMask & CLASSMASK_MONK) != 0
        && ((nNotUsable2 & nSpecialization) & SPECMASK_MONK) == 0) {
        return 1;
    }

    if ((nNotUsable1 & 0x40) == 0
        && (nClassMask & CLASSMASK_PALADIN) != 0
        && ((nNotUsable2 & nSpecialization) & SPECMASK_PALADIN) == 0) {
        return 1;
    }

    if ((nNotUsable1 & 0x80) == 0
        && (nClassMask & CLASSMASK_RANGER) != 0) {
        return 1;
    }

    if ((nNotUsable1 & 0x100) == 0
        && (nClassMask & CLASSMASK_ROGUE) != 0) {
        return 1;
    }

    if ((nNotUsable1 & 0x200) == 0
        && (nClassMask & CLASSMASK_SORCERER) != 0) {
        return 1;
    }

    if ((nNotUsable1 & 0x400) == 0
        && (nClassMask & CLASSMASK_WIZARD) != 0
        && ((nNotUsable2 & nSpecialization) & SPECMASK_WIZARD) == 0) {
        return 1;
    }

    if ((nClassMask & CLASSMASK_ROGUE) != 0
        && pSprite->GetDerivedStats()->m_nSkills[CGAMESPRITE_SKILL_USE_MAGIC_DEVICE] > 0) {
        return 2;
    }

    if ((nClassMask & CLASSMASK_BARD) != 0
        && pSprite->GetDerivedStats()->m_nSkills[CGAMESPRITE_SKILL_USE_MAGIC_DEVICE] > 0) {
        return 2;
    }

    return 0;
}

// 0x5BACE0
SHORT CInfGame::GetNumQuickWeaponSlots(SHORT nPortrait)
{
    SHORT nSlots;

    // NOTE: Uninline.
    LONG nCharacterId = GetCharacterId(nPortrait);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = m_cObjectArray.GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        nSlots = static_cast<SHORT>(pSprite->GetNumQuickWeaponSlots());
    } else {
        nSlots = 0;
    }

    m_cObjectArray.ReleaseShare(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return nSlots;
}

// 0x5BAD70
BOOL CInfGame::SwapItemPersonalInventory(SHORT nPortraitNum, CItem*& pNewItem, SHORT nSlotNum, STRREF& errorCode, WORD wCount, BOOLEAN bFromServer, BOOL bAutoStacking)
{
    // NOTE: Uninline.
    LONG nCharacterId = GetCharacterId(nPortraitNum);

    BOOL bResult = TRUE;
    errorCode = -1;

    if ((nSlotNum < 18 || nSlotNum >= 18 + CScreenInventory::PERSONAL_INVENTORY_SIZE)
        && nSlotNum != 0x7FFF) {
        // "Bad Slot Number"
        errorCode = 9374;
        return FALSE;
    }

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        // "No Sprite"
        errorCode = 9373;
        return FALSE;
    }

    if (g_pChitin->cNetwork.m_nServiceProvider != 0
        && g_pChitin->cNetwork.m_idLocalPlayer != pSprite->m_remotePlayerID
        && g_pChitin->cNetwork.m_bConnectionEstablished == TRUE
        && !bFromServer) {
        // NOTE: A client asks the server for the swap here (0x431560 on
        // `m_cBaldurMessage` with the character, the slot, the item, the
        // error and the count) and returns its answer
        // (0x5BAE6D-0x5BAEBC). The branch needs a provider other than the
        // null one, so a single-player host session never takes it; the
        // send is not ported.
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return FALSE;
    }

    CItem* pSlotItem;

    if (nSlotNum != 0x7FFF
        && pNewItem != NULL
        && (pSlotItem = pSprite->m_equipment.m_items[nSlotNum]) != NULL
        && pSlotItem->GetItemType() == 58) {
        // Into a bag.
        INT nAdded = AddItemToBag(pSlotItem->GetResRef(), pNewItem, errorCode);
        if (nAdded != 0) {
            if (pNewItem->GetMaxStackable() > 1 && pNewItem->GetUsageCount(0) != nAdded) {
                pNewItem->SetUsageCount(0, pNewItem->GetUsageCount(0) - nAdded);
            } else {
                // NOTE: Uninline.
                AddDisposableItem(pNewItem);
                pNewItem = NULL;
            }

            g_pBaldurChitin->GetActiveEngine()->UpdatePersonalItemStatus(pSprite->m_id);

            if ((g_pChitin->cNetwork.m_nServiceProvider == 0
                    || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID)
                && g_pChitin->cNetwork.m_bConnectionEstablished == TRUE) {
                CMessageSpriteEquipment* pMessage = new CMessageSpriteEquipment(pSprite,
                    nCharacterId,
                    nCharacterId);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
            }

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            return TRUE;
        }

        bResult = FALSE;
    } else if (bAutoStacking && pNewItem != NULL && pNewItem->GetMaxStackable() > 1) {
        for (INT nSlot = 18; static_cast<SHORT>(nSlot) < 18 + CScreenInventory::PERSONAL_INVENTORY_SIZE; nSlot++) {
            pSlotItem = pSprite->m_equipment.m_items[nSlot];
            if (pSlotItem != NULL
                && pSlotItem->GetResRef() == pNewItem->GetResRef()
                && ((pSlotItem->m_flags ^ pNewItem->m_flags) & 0x1) == 0) {
                if (pSlotItem->GetUsageCount(0) < pSlotItem->GetMaxStackable()) {
                    if (pSlotItem->GetUsageCount(0) + pNewItem->GetUsageCount(0) <= pSlotItem->GetMaxStackable()) {
                        nSlotNum = static_cast<SHORT>(nSlot);
                        break;
                    }

                    pNewItem->SetUsageCount(0, pNewItem->GetUsageCount(0) - pSlotItem->GetMaxStackable() + pSlotItem->GetUsageCount(0));
                    pSlotItem->SetUsageCount(0, pSlotItem->GetMaxStackable());
                }
            }
        }
    }

    if (nSlotNum == 0x7FFF) {
        for (INT nSlot = 18; static_cast<SHORT>(nSlot) < 18 + CScreenInventory::PERSONAL_INVENTORY_SIZE && nSlotNum == 0x7FFF; nSlot++) {
            if (pSprite->m_equipment.m_items[static_cast<SHORT>(nSlot)] == NULL) {
                nSlotNum = static_cast<SHORT>(nSlot);
            }
        }

        if (nSlotNum == 0x7FFF) {
            // "Inventory Is Full"
            errorCode = 17999;
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            return FALSE;
        }
    }

    pSlotItem = pSprite->m_equipment.m_items[nSlotNum];

    if (bResult) {
        if (pSlotItem != NULL
            && ((pSlotItem->GetFlagsFile() & 0x4) == 0 || (pSlotItem->m_flags & 0x10) != 0)) {
            bResult = FALSE;
            // "This item can not be removed."
            errorCode = 25697;
        } else {
            if (pSlotItem != NULL
                && pNewItem == NULL
                && pSlotItem->GetMaxStackable() > 1
                && wCount < pSlotItem->GetUsageCount(0)) {
                // Take part of the stack.

                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
                // __LINE__: 14884
                UTIL_ASSERT(wCount != 0);

                pSlotItem->SetUsageCount(0, pSlotItem->GetUsageCount(0) - wCount);
                pNewItem = new CItem(pSlotItem->GetResRef(), 0, 0, 0, 0, 0);
                pNewItem->SetUsageCount(0, wCount);
                pNewItem->m_flags = ((pSlotItem->m_flags ^ pNewItem->m_flags) & 0x1) ^ pNewItem->m_flags;
            } else if (pSlotItem != NULL
                && pNewItem != NULL
                && pSlotItem->GetMaxStackable() > 1
                && pNewItem->GetResRef() == pSlotItem->GetResRef()
                && ((pNewItem->m_flags ^ pSlotItem->m_flags) & 0x1) == 0) {
                // Onto a stack of the same item.

                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
                // __LINE__: 14894
                UTIL_ASSERT(wCount != 0);

                INT nMove = pNewItem->GetUsageCount(0) < pSlotItem->GetMaxStackable() - pSlotItem->GetUsageCount(0)
                    ? pNewItem->GetUsageCount(0)
                    : pSlotItem->GetMaxStackable() - pSlotItem->GetUsageCount(0);
                if (wCount < nMove) {
                    nMove = wCount;
                } else {
                    nMove = pNewItem->GetUsageCount(0) < pSlotItem->GetMaxStackable() - pSlotItem->GetUsageCount(0)
                        ? pNewItem->GetUsageCount(0)
                        : pSlotItem->GetMaxStackable() - pSlotItem->GetUsageCount(0);
                }

                pSlotItem->SetUsageCount(0, pSlotItem->GetUsageCount(0) + nMove);

                if (pNewItem->GetUsageCount(0) == static_cast<WORD>(nMove)) {
                    // NOTE: Uninline.
                    AddDisposableItem(pNewItem);
                    pNewItem = NULL;
                } else {
                    pNewItem->SetUsageCount(0, pNewItem->GetUsageCount(0) - nMove);
                }
            } else {
                pSprite->m_equipment.m_items[nSlotNum] = pNewItem;
                pNewItem = pSlotItem;
            }

            g_pBaldurChitin->GetActiveEngine()->UpdatePersonalItemStatus(pSprite->m_id);

            if ((g_pChitin->cNetwork.m_nServiceProvider == 0
                    || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID)
                && g_pChitin->cNetwork.m_bConnectionEstablished == TRUE) {
                CMessageSpriteEquipment* pMessage = new CMessageSpriteEquipment(pSprite,
                    nCharacterId,
                    nCharacterId);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
            }
        }
    }

    pSprite->field_562C = 1;
    pSprite->ProcessEffectList();

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return bResult;
}

// 0x5BB600
void CInfGame::InventoryInfoPersonal(SHORT nPortraitNum, SHORT nSlotNum, CItem*& pItem, STRREF& description, CResRef& cResIcon, CResRef& cResItem, WORD& wCount, BOOL a8)
{
    // NOTE: Uninline.
    LONG nCharacterId = GetCharacterId(nPortraitNum);

    description = -1;
    cResIcon = "";
    cResItem = "";
    wCount = 0;

    if (nSlotNum < CGameSpriteEquipment::NUM_SLOT) {
        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            if (nSlotNum == 43
                || nSlotNum == 45
                || nSlotNum == 47
                || nSlotNum == 49) {
                if (pSprite->GetEquipment()->m_items[nSlotNum] == NULL && a8 == TRUE) {
                    if (pSprite->GetEquipment()->m_items[nSlotNum + 1] != NULL) {
                        // FIXME: Calls `GetItemType` four times.
                        if (pSprite->GetEquipment()->m_items[nSlotNum + 1]->GetItemType() == 47
                            || pSprite->GetEquipment()->m_items[nSlotNum + 1]->GetItemType() == 53
                            || pSprite->GetEquipment()->m_items[nSlotNum + 1]->GetItemType() == 49
                            || pSprite->GetEquipment()->m_items[nSlotNum + 1]->GetItemType() == 41) {
                            nSlotNum = 10;
                        }
                    } else {
                        if (pSprite->m_nWeaponSet == (nSlotNum - 43) / 2) {
                            nSlotNum = 10;
                        }
                    }
                }
            }

            pItem = pSprite->GetEquipment()->m_items[nSlotNum];
            if (pItem != NULL) {
                description = pItem->GetGenericName();
                cResIcon = pItem->GetItemIcon();
                cResItem = pItem->GetResRef();

                if (pItem->GetMaxStackable() > 1) {
                    wCount = pItem->GetUsageCount(0);
                }
            }

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x5BB800
BOOL CInfGame::Is3DSound(int nSoundChannel)
{
    return m_ruleTables.Is3DSound(nSoundChannel);
}

// 0x5BB810
float CInfGame::GetSoundReverbMix(int nSoundChannel, int nReverb)
{
    return m_ruleTables.GetSoundReverbMix(nSoundChannel, nReverb);
}

// 0x5BB830
CResRef CInfGame::GetAnimationBam(SHORT nPortrait, BYTE range)
{
    CString sResRef;

    // NOTE: Uninline.
    LONG nCharacterId = GetCharacterId(nPortrait);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = m_cObjectArray.GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return CResRef("");
    }

    pSprite->GetAnimation()->GetAnimationResRef(sResRef, range);

    m_cObjectArray.ReleaseShare(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return CResRef(sResRef);
}

// 0x5BB960
BYTE CInfGame::GetFrameRate()
{
    return static_cast<BYTE>(g_pBaldurChitin->GetActiveEngine()->pVidMode->field_94);
}

// 0x5BE900
void CInfGame::SynchronousUpdate()
{
    if (field_366E) {
        CResHelper<CResBitmap, 1> cScreenFile;
        CResHelper<CResBitmap, 1> cPortraitFile;
        CVidBitmap vbPortrait;
        BYTE* pScreenData = NULL;
        BYTE* pPortraitData = NULL;
        CString sDir;

        if (field_50D8 && !field_50DC) {
            CString sSource;
            CString sDest;

            sSource = GetDirSaveRoot() + "default\\" + "ICEWIND2.BMP";
            sDest = GetDirSave() + "ICEWIND2.BMP";
            CopyFileA(sSource, sDest, FALSE);
            DeleteFileA(sSource);

            for (INT nPortrait = 0; nPortrait < 6; nPortrait++) {
                sSource.Format("%sdefault\\%s%d.BMP", (LPCSTR)GetDirSaveRoot(), "PORTRT", nPortrait);
                sDest.Format("%s%s%d.BMP", (LPCSTR)GetDirSave(), "PORTRT", nPortrait);
                CopyFileA(sSource, sDest, FALSE);
                DeleteFileA(sSource);
            }

            field_366E = 0;
            field_50D8 = FALSE;
            return;
        }

        CGameArea* pVisibleArea = m_gameAreas[m_visibleArea];
        if (pVisibleArea != NULL) {
            EnterCriticalSection(&(pVisibleArea->field_214));
            pVisibleArea->SetListManipulationThreadId(GetCurrentThreadId());
        }

        EnterCriticalSection(&(m_gameAreas[m_visibleArea]->field_1FC));

        Sleep(100);

        CSingleLock lock(&(g_pBaldurChitin->m_pEngineWorld->GetManager()->field_36), FALSE);
        lock.Lock(INFINITE);

        CVidMode* pVidMode = g_pChitin->pActiveEngine != NULL ? g_pChitin->pActiveEngine->pVidMode : NULL;
        BYTE nBrightnessCorrection = pVidMode->m_nBrightnessCorrection;
        pVidMode->m_nBrightnessCorrection = 0;
        BYTE nGammaCorrection = pVidMode->m_nGammaCorrection;
        pVidMode->m_nGammaCorrection = 0;

        m_gameAreas[m_visibleArea]->m_cInfinity.pVRPool->InvalidateAll();
        pVidMode->LoadFogOWarSurfaces(FOGOWAR_RESREF);

        field_366E = 0;

        BOOL bCacheStatus = CCacheStatus::dword_8D0BA8;
        CCacheStatus::dword_8D0BA8 = FALSE;
        g_pBaldurChitin->m_pEngineWorld->SaveScreen();
        CCacheStatus::dword_8D0BA8 = bCacheStatus;

        LONG nScreenData = 0;
        pVidMode->PrintSurfaceToBmp(pScreenData, 0, CInfinity::stru_8E79A8, nScreenData, 5);

        if (!field_50DC && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
            sDir = GetDirSave();
        } else if (!g_pChitin->cVideo.m_bIs3dAccelerated) {
            sDir = GetDirSaveRoot() + "default\\";
        } else if (field_50DC) {
            sDir = GetDirSave();
            field_50DC = FALSE;
            field_50D8 = FALSE;
        } else {
            sDir = GetDirSaveRoot() + "default\\";
            field_50D8 = TRUE;
        }

        if (pScreenData != NULL) {
            // NOTE: Uninline.
            cScreenFile.SetResRef(CResRef("ICEWIND2"), TRUE, TRUE);
            static_cast<CRes*>(cScreenFile.GetRes())->Write(sDir, pScreenData, nScreenData);
            delete[] pScreenData;
        }

        WORD nPortrait = 0;
        for (INT nCharacter = 0; nCharacter < 6; nCharacter++) {
            LONG nCharacterId = m_characters[nCharacter];
            if (nCharacterId == CGameObjectArray::INVALID_INDEX) {
                continue;
            }

            CGameSprite* pSprite;
            BYTE rc;
            do {
                rc = m_cObjectArray.GetShare(nCharacterId,
                    CGameObjectArray::THREAD_1,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc != CGameObjectArray::SUCCESS) {
                continue;
            }

            vbPortrait.SetResRef(CResRef(pSprite->m_baseStats.m_portraitSmall), TRUE, TRUE);
            vbPortrait.m_bDoubleSize = FALSE;

            if (vbPortrait.GetRes() == NULL) {
                vbPortrait.SetResRef(CResRef(SILHOUETTE_PORTRAIT_SM), TRUE, TRUE);
                vbPortrait.m_bDoubleSize = g_pBaldurChitin->field_4A28;
            }

            if (vbPortrait.GetBitCount(FALSE) != 24 && vbPortrait.GetBitCount(FALSE) != 8) {
                vbPortrait.SetResRef(CResRef(SILHOUETTE_PORTRAIT_SM), TRUE, TRUE);
                vbPortrait.m_bDoubleSize = g_pBaldurChitin->field_4A28;
            }

            m_cObjectArray.ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_1,
                INFINITE);

            CSize size;
            vbPortrait.GetImageDimensions(size, FALSE);

            CRect rPortrait;
            SetRect(&rPortrait, 0, 0, size.cx, size.cy);

            vbPortrait.RenderDirect(2, 0, 0, rPortrait, 0, FALSE);

            LONG nPortraitData = 0;
            pPortraitData = NULL;
            pVidMode->PrintSurfaceToBmp(pPortraitData, 2, rPortrait, nPortraitData, 2);

            if (pPortraitData != NULL) {
                CString sPortrait;
                sPortrait.Format("%s%d", "PORTRT", nPortrait);

                // NOTE: Uninline.
                cPortraitFile.SetResRef(CResRef(sPortrait), TRUE, TRUE);
                static_cast<CRes*>(cPortraitFile.GetRes())->Write(sDir, pPortraitData, nPortraitData);

                nPortrait++;
                delete[] pPortraitData;
            }
        }

        g_pChitin->cDimm.RemoveFromDirectoryList(GetDirSave(), TRUE);

        pVidMode->m_nBrightnessCorrection = nBrightnessCorrection;
        pVidMode->m_nGammaCorrection = nGammaCorrection;

        m_gameAreas[m_visibleArea]->m_cInfinity.pVRPool->InvalidateAll();
        pVidMode->LoadFogOWarSurfaces(FOGOWAR_RESREF);

        lock.Unlock();

        LeaveCriticalSection(&(m_gameAreas[m_visibleArea]->field_1FC));

        pVisibleArea = m_gameAreas[m_visibleArea];
        if (pVisibleArea != NULL) {
            pVisibleArea->SetListManipulationThreadId(0);
            LeaveCriticalSection(&(pVisibleArea->field_214));
        }
    }

    DestroyDisposableItems();
}

// 0x5C0520
CStringList* CInfGame::GetSaveGames()
{
    CStringList* pList = new CStringList();

    CString sFileName;

    WIN32_FIND_DATAA ffd;
    HANDLE hFindFile = FindFirstFileA(GetDirSaveRoot() + "*.*", &ffd);
    if (hFindFile != INVALID_HANDLE_VALUE) {
        do {
            sFileName = ffd.cFileName;
            if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0
                && sFileName != "."
                && sFileName != ".."
                && SaveGameExists(sFileName)) {
                POSITION pos = pList->GetHeadPosition();
                while (pos != NULL) {
                    if (sFileName < pList->GetAt(pos)) {
                        break;
                    }
                    pList->GetNext(pos);
                }

                if (pos != NULL) {
                    pList->InsertBefore(pos, sFileName);
                } else {
                    pList->AddTail(sFileName);
                }
            }
        } while (FindNextFileA(hFindFile, &ffd));

        FindClose(hFindFile);
    }

    return pList;
}

// 0x5C0730
BOOL CInfGame::DeleteSaveGame(const CString& sFileName)
{
    CString sDirectoryName = GetDirSaveRoot() + sFileName;

    CFileStatus cFileStatus;
    if (!CFile::GetStatus(sDirectoryName, cFileStatus)) {
        return FALSE;
    }

    sDirectoryName += "\\";
    if (!g_pChitin->cDimm.DirectoryRemoveFiles(sDirectoryName)) {
        return FALSE;
    }

    WIN32_FIND_DATAA ffd;
    HANDLE hFindFile = FindFirstFileA(sDirectoryName + "*.*", &ffd);
    if (hFindFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    do {
        if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || ffd.cFileName[0] != '.') {
            FindClose(hFindFile);
            return FALSE;
        }
    } while (FindNextFileA(hFindFile, &ffd));

    FindClose(hFindFile);

    return RemoveDirectoryA(sDirectoryName);
}

// 0x5C0890
BOOL CInfGame::SaveGameExists(const CString& sSaveGame)
{
    CString sFileName = GetDirSaveRoot() + sSaveGame + "\\ICEWIND2.GAM";

    HANDLE hFile = CreateFileA(sFileName,
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    // NOTE: Original code is slightly different (conditions inverted).
    BOOL bSuccess = FALSE;
    if (hFile != INVALID_HANDLE_VALUE) {
        unsigned char signature[8];
        DWORD dwBytesRead;
        ReadFile(hFile, signature, sizeof(signature), &dwBytesRead, NULL);
        if (dwBytesRead == sizeof(signature)
            && memcmp(signature, "GAMEV2.2", sizeof(signature)) == 0) {
            bSuccess = TRUE;
        }

        CloseHandle(hFile);
    }

    return bSuccess;
}

// 0x5C0EA0
void CInfGame::ApplyVolumeSliders(BOOLEAN a2)
{
    g_pBaldurChitin->cSoundMixer.SetMusicVolume(m_cOptions.m_nVolumeMusic);

    g_pBaldurChitin->cSoundMixer.SetChannelVolumeFast(0, m_cOptions.m_nVolumeVoices);
    g_pBaldurChitin->cSoundMixer.SetChannelVolumeFast(1, m_cOptions.m_nVolumeAmbients);
    g_pBaldurChitin->cSoundMixer.SetChannelVolumeFast(2, m_cOptions.m_nVolumeSFX);
    g_pBaldurChitin->cSoundMixer.SetChannelVolumeFast(3, m_cOptions.m_nVolumeSFX);
    g_pBaldurChitin->cSoundMixer.SetChannelVolumeFast(4, m_cOptions.m_nVolumeSFX);
    g_pBaldurChitin->cSoundMixer.SetChannelVolumeFast(5, m_cOptions.m_nVolumeSFX);
    g_pBaldurChitin->cSoundMixer.SetChannelVolumeFast(2, m_cOptions.m_nVolumeVoices);

    for (int k = 7; k < 13; k++) {
        g_pBaldurChitin->cSoundMixer.SetChannelVolumeFast(k, m_cOptions.m_nVolumeVoices);
    }

    g_pBaldurChitin->cSoundMixer.SetChannelVolumeFast(13, m_cOptions.m_nVolumeSFX);
    g_pBaldurChitin->cSoundMixer.SetChannelVolumeFast(14, m_cOptions.m_nVolumeSFX);
    g_pBaldurChitin->cSoundMixer.SetChannelVolumeFast(15, m_cOptions.m_nVolumeSFX);
    g_pBaldurChitin->cSoundMixer.SetChannelVolumeFast(20, m_cOptions.m_nVolumeSFX);
    g_pBaldurChitin->cSoundMixer.SetChannelVolumeFast(18, m_cOptions.m_nVolumeSFX);
    g_pBaldurChitin->cSoundMixer.SetChannelVolumeFast(19, m_cOptions.m_nVolumeSFX);

    if (a2 == TRUE) {
        g_pBaldurChitin->cSoundMixer.SetChannelVolumeFast(16, m_cOptions.m_nVolumeAmbients);
        g_pBaldurChitin->cSoundMixer.SetChannelVolumeFast(17, m_cOptions.m_nVolumeAmbients);
    }

    g_pBaldurChitin->cSoundMixer.UpdateSoundList();
}

// 0x5BB980
void CInfGame::UseMagicOnGround(CPoint pt)
{
    CAIAction action;

    LONG nCharacterId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_group.m_memberList.GetHead()));

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        // NOTE: Uninline.
        g_pBaldurChitin->GetObjectGame()->SetLastClick(CPoint(-1, -1));

        // NOTE: Uninline.
        g_pBaldurChitin->GetObjectGame()->SetLastTarget(CGameObjectArray::INVALID_INDEX);

        BOOL addAction = FALSE;
        switch (pSprite->m_currentUseButton.m_abilityId.m_itemType) {
        case 2:
            action = CAIAction(CAIAction::USEITEMPOINT,
                pt,
                pSprite->m_currentUseButton.m_abilityId.m_itemNum,
                pSprite->m_currentUseButton.m_abilityId.m_abilityNum);
            addAction = TRUE;
            break;
        case 3:
            if (1) {
                pSprite->m_castCounter = -1;

                CString string;
                pSprite->m_currentUseButton.m_abilityId.m_res.CopyToString(string);

                action = CAIAction(CAIAction::SPELLPOINTNODEC,
                    string,
                    pt,
                    0,
                    pSprite->m_currentUseButton.m_abilityId.m_nClass | (pSprite->m_currentUseButton.m_abilityId.field_1E << 8));
                action.m_specificID3 = pSprite->m_currentUseButton.m_abilityId.field_1D;
                addAction = TRUE;
            }
            break;
        case 4:
            if (1) {
                CString string;
                pSprite->m_currentUseButton.m_abilityId.m_res.CopyToString(string);

                action = CAIAction(CAIAction::FORCESPELLPOINT,
                    string,
                    pt,
                    0,
                    pSprite->m_currentUseButton.m_abilityId.m_nClass | (pSprite->m_currentUseButton.m_abilityId.field_1E << 8));
                action.m_specificID3 = pSprite->m_currentUseButton.m_abilityId.field_1D;
                addAction = TRUE;
            }
            break;
        case 5:
            pSprite->FireSpellPoint(pSprite->m_currentUseButton.m_abilityId.m_res, pt);
            break;
        default:
            if (1) {
                CString string;
                pSprite->m_currentUseButton.m_abilityId.m_res.CopyToString(string);

                action = CAIAction(CAIAction::SPELLPOINT,
                    string,
                    pt,
                    0,
                    pSprite->m_currentUseButton.m_abilityId.m_nClass | (pSprite->m_currentUseButton.m_abilityId.field_1E << 8));
                action.m_specificID3 = pSprite->m_currentUseButton.m_abilityId.field_1D;
                addAction = TRUE;
            }
            break;
        }

        if (addAction) {
            pSprite->ClearActions(FALSE);
            pSprite->m_userCommandPause = CGameSprite::USER_OVERRIDE_COUNT;
            pSprite->m_triggerId = -1;
            pSprite->AddAction(action);
            pSprite->m_interrupt = TRUE;
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x5BC450
void CInfGame::UseMagicOnObject(LONG target)
{
    CAIAction action;
    CAIObjectType targetType(0, 0, 0, 0, 0, 0, 0, 0, target, 0, 0);

    LONG nCharacterId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_group.m_memberList.GetHead()));

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        // NOTE: Uninline.
        g_pBaldurChitin->GetObjectGame()->SetLastClick(CPoint(-1, -1));

        // NOTE: Uninline.
        g_pBaldurChitin->GetObjectGame()->SetLastTarget(CGameObjectArray::INVALID_INDEX);

        BOOL addAction = FALSE;
        switch (pSprite->m_currentUseButton.m_abilityId.m_itemType) {
        case 2:
            action = CAIAction(CAIAction::USEITEM,
                targetType,
                pSprite->m_currentUseButton.m_abilityId.m_itemNum,
                pSprite->m_currentUseButton.m_abilityId.m_abilityNum,
                0);
            addAction = TRUE;
            break;
        case 3:
            if (1) {
                pSprite->m_castCounter = -1;

                CString string;
                pSprite->m_currentUseButton.m_abilityId.m_res.CopyToString(string);

                action = CAIAction(CAIAction::SPELLNODEC,
                    targetType,
                    string,
                    0,
                    pSprite->m_currentUseButton.m_abilityId.m_nClass | (pSprite->m_currentUseButton.m_abilityId.field_1E << 8));
                action.m_specificID3 = pSprite->m_currentUseButton.m_abilityId.field_1D;
                addAction = TRUE;
            }
            break;
        case 4:
            if (1) {
                CString string;
                pSprite->m_currentUseButton.m_abilityId.m_res.CopyToString(string);

                action = CAIAction(CAIAction::FORCESPELL,
                    targetType,
                    string,
                    0,
                    pSprite->m_currentUseButton.m_abilityId.m_nClass | (pSprite->m_currentUseButton.m_abilityId.field_1E << 8));
                action.m_specificID3 = pSprite->m_currentUseButton.m_abilityId.field_1D;
                addAction = TRUE;
            }
            break;
        case 5:
            if (1) {
                CGameObject* pTarget;

                BYTE rc;
                do {
                    rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(target,
                        CGameObjectArray::THREAD_ASYNCH,
                        &pTarget,
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                if (rc == CGameObjectArray::SUCCESS) {
                    pSprite->FireSpell(pSprite->m_currentUseButton.m_abilityId.m_res, pTarget);

                    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(target,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                }
            }
            break;
        default:
            if (1) {
                CString string;
                pSprite->m_currentUseButton.m_abilityId.m_res.CopyToString(string);

                action = CAIAction(CAIAction::SPELL,
                    targetType,
                    string,
                    0,
                    pSprite->m_currentUseButton.m_abilityId.m_nClass | (pSprite->m_currentUseButton.m_abilityId.field_1E << 8));
                action.m_specificID3 = pSprite->m_currentUseButton.m_abilityId.field_1D;
                addAction = TRUE;
            }
            break;
        }

        if (addAction) {
            pSprite->ClearActions(FALSE);
            pSprite->m_userCommandPause = CGameSprite::USER_OVERRIDE_COUNT;
            pSprite->m_triggerId = -1;
            pSprite->AddAction(action);
            pSprite->m_interrupt = TRUE;
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x5BD070
BOOL CInfGame::AddCharacterToParty(LONG id, SHORT nPortraitNumber)
{
    // NOTE: Uninline.
    if (GetCharacterPortraitNum(id) != -1) {
        return FALSE;
    }

    if (m_nCharacters >= CINFGAME_MAXCHARACTERS) {
        return FALSE;
    }

    if (m_allies.Find(reinterpret_cast<int*>(id)) != NULL) {
        // NOTE: Uninline.
        RemoveCharacterFromAllies(id);
    }

    if (m_familiars.Find(reinterpret_cast<int*>(id)) != NULL) {
        // NOTE: Uninline.
        RemoveCharacterFromFamiliars(id);
    }

    POSITION pos = m_lstGlobalCreatures.GetHeadPosition();
    while (pos != NULL) {
        POSITION posOld = pos;
        if (static_cast<int>(reinterpret_cast<INT_PTR>(m_lstGlobalCreatures.GetNext(pos))) == id) {
            m_lstGlobalCreatures.RemoveAt(posOld);
            break;
        }
    }

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(id,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    if (nPortraitNumber == -1 || nPortraitNumber >= CINFGAME_MAXCHARACTERS) {
        nPortraitNumber = m_nCharacters;
    }

    m_characterPortraits[nPortraitNumber] = id;

    for (INT nCharacterSlot = 0; nCharacterSlot < CINFGAME_MAXCHARACTERS; nCharacterSlot++) {
        if (m_characters[nCharacterSlot] == CGameObjectArray::INVALID_INDEX) {
            m_characters[nCharacterSlot] = id;
            break;
        }
    }

    UpdatePortraitToolTip(nPortraitNumber, pSprite->GetNameRef());
    pSprite->SetFootstepChannel();

    if ((pSprite->GetBaseStats()->m_generalState & STATE_DEAD) != 0) {
        pSprite->m_canBeSeen = 4 * (CGameObject::VISIBLE_DELAY + 1);
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(id,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    EnablePortrait(static_cast<BYTE>(nPortraitNumber), TRUE);

    m_nCharacters++;

    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    pWorld->GetManager()->GetPanel(pWorld->GetPanel_22_0())->InvalidateRect(NULL);

    return TRUE;
}

// 0x5BD270
BOOL CInfGame::RemoveCharacterFromParty(LONG id, BOOL& overflow)
{
    CGameSprite* pSprite;
    BYTE rc;
    SHORT cnt;

    // NOTE: Uninline.
    SHORT nPortrait = GetCharacterPortraitNum(id);

    overflow = FALSE;

    if (nPortrait == -1) {
        for (int index = 0; index < m_characterOverflow.GetCount(); index++) {
            if (static_cast<int>(reinterpret_cast<INT_PTR>(m_characterOverflow.GetAt(index))) == id) {
                do {
                    rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(id,
                        CGameObjectArray::THREAD_ASYNCH,
                        reinterpret_cast<CGameObject**>(&pSprite),
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                if (rc == CGameObjectArray::SUCCESS) {
                    overflow = TRUE;
                    m_characterOverflow.RemoveAt(index);

                    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(id,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);

                    return TRUE;
                }
            }
        }
        return FALSE;
    }

    for (cnt = 0; cnt < CINFGAME_MAXCHARACTERS; cnt++) {
        if (m_characters[cnt] == id) {
            m_characters[cnt] = -1;
        }
    }

    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(id,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    m_group.Remove(pSprite);
    pSprite->SetFootstepChannel();
    pSprite->m_cGameStats.RecordLeaveParty();

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(id,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    for (cnt = 0; cnt < m_nCharacters - 1; cnt++) {
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(m_characterPortraits[cnt],
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            UpdatePortraitToolTip(cnt, pSprite->GetNameRef());
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(m_characterPortraits[cnt],
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    m_lstGlobalCreatures.AddHead(reinterpret_cast<int*>(id));

    m_nCharacters--;
    m_characterPortraits[m_nCharacters] = -1;

    EnablePortrait(static_cast<BYTE>(m_nCharacters), FALSE);

    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    pWorld->GetManager()->GetPanel(pWorld->GetPanel_22_0())->InvalidateRect(NULL);

    return TRUE;
}

// 0x5BD4E0
BOOL CInfGame::SetCharacterSlot(INT nCharacterSlot, LONG nCharacterId)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 15670
    UTIL_ASSERT(0 <= nCharacterSlot && nCharacterSlot < CINFGAME_MAXCHARACTERS);

    if (m_characters[nCharacterSlot] != -1) {
        return FALSE;
    }

    m_characters[nCharacterSlot] = nCharacterId;

    return TRUE;
}

// 0x5BD530
BOOL CInfGame::ClearCharacterSlot(INT nCharacterSlot)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 15717
    UTIL_ASSERT(0 <= nCharacterSlot && nCharacterSlot < CINFGAME_MAXCHARACTERS);

    LONG id = m_characters[nCharacterSlot];
    if (id == -1) {
        return FALSE;
    }

    m_characters[nCharacterSlot] = -1;

    BOOL overflow;
    RemoveCharacterFromParty(id, overflow);

    POSITION pos = m_lstGlobalCreatures.GetHeadPosition();
    while (pos != NULL) {
        POSITION posOld = pos;
        if (static_cast<LONG>(reinterpret_cast<INT_PTR>(m_lstGlobalCreatures.GetNext(pos))) == id) {
            m_lstGlobalCreatures.RemoveAt(posOld);
            break;
        }
    }

    CGameSprite* pSprite;
    if (m_cObjectArray.GetDeny(id, CGameObjectArray::THREAD_ASYNCH, reinterpret_cast<CGameObject**>(&pSprite), INFINITE) != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    if (pSprite->GetArea() != NULL) {
        pSprite->GetArea()->m_visibility.RemoveCharacter(pSprite->GetPos(),
            pSprite->GetId(),
            pSprite->GetTerrainTable());

        // The BYTE is compared with 0xFF (`cmp al, 0xff` at 0x5BD61E).
        if (pSprite->GetArea()->m_nCharacters != 0xFF) {
            pSprite->GetArea()->m_nCharacters--;
        }

        pSprite->RemoveFromArea();
    }

    m_cObjectArray.ReleaseDeny(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);

    return TRUE;
}

// 0x5BD650
LONG CInfGame::GetCharacterSlot(INT nCharacterSlot)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 15787
    UTIL_ASSERT(0 <= nCharacterSlot && nCharacterSlot < CINFGAME_MAXCHARACTERS);

    return m_characters[nCharacterSlot];
}

// 0x5BD690
INT CInfGame::GetCharacterSlotFromId(LONG nCharacterId)
{
    for (int index = 0; index < CINFGAME_MAXCHARACTERS; index++) {
        if (m_characters[index] == nCharacterId) {
            return index;
        }
    }

    return -1;
}

// 0x5BD6B0
void CInfGame::UpdateCharacterSlots()
{
    INT nSlot;
    INT nIndex;
    INT nPortraits;
    LONG nCharacterId;
    CGameSprite* pSprite;
    BYTE rc;

    m_nCharacters = 0;

    for (nSlot = 0; nSlot < 6; nSlot++) {
        nCharacterId = m_characters[nSlot];
        if (nCharacterId != CGameObjectArray::INVALID_INDEX
            && (!g_pChitin->cNetwork.GetSessionOpen()
                || m_multiplayerSettings.GetCharacterStatus(nSlot) == CMultiplayerSettings::CHARSTATUS_CHARACTER)) {
            do {
                rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc != CGameObjectArray::SUCCESS) {
                return;
            }

            m_characters[m_nCharacters] = nCharacterId;

            if (g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
                m_multiplayerSettings.SetCharacterControlledByPlayer(m_nCharacters,
                    m_multiplayerSettings.GetCharacterControlledByPlayer(nSlot),
                    FALSE,
                    FALSE);

                if (m_multiplayerSettings.GetCharacterStatus(nSlot) == CMultiplayerSettings::CHARSTATUS_CHARACTER) {
                    m_multiplayerSettings.SignalCharacterStatus(m_nCharacters,
                        CMultiplayerSettings::CHARSTATUS_SIGNAL_IMPORTED,
                        TRUE,
                        FALSE);
                } else {
                    m_multiplayerSettings.SignalCharacterStatus(m_nCharacters,
                        CMultiplayerSettings::CHARSTATUS_SIGNAL_DELETED,
                        TRUE,
                        FALSE);
                }

                m_multiplayerSettings.SetCharacterReady(m_nCharacters, TRUE, FALSE);
            }

            pSprite->m_cGameStats.RecordJoinParty();
            EnablePortrait(static_cast<BYTE>(m_nCharacters), TRUE);
            UpdatePortraitToolTip(m_nCharacters, pSprite->GetNameRef());

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            m_nCharacters++;
        }
    }

    // Keep the portrait order of the characters still in the party.
    nPortraits = 0;
    for (nSlot = 0; nSlot < 6; nSlot++) {
        for (nIndex = 0; nIndex < m_nCharacters; nIndex++) {
            if (m_characters[nIndex] == m_characterPortraits[nSlot]) {
                m_characterPortraits[nPortraits] = m_characters[nIndex];
                nPortraits++;
                break;
            }
        }
    }

    // Append the characters that have no portrait yet.
    for (nIndex = 0; nPortraits < m_nCharacters; nIndex++) {
        if (nIndex >= m_nCharacters) {
            break;
        }

        for (nSlot = 0; nSlot < 6; nSlot++) {
            if (m_characters[nIndex] == m_characterPortraits[nSlot]) {
                break;
            }
        }

        if (nSlot == 6) {
            m_characterPortraits[nPortraits] = m_characters[nIndex];
            nPortraits++;
        }
    }

    for (nSlot = m_nCharacters; nSlot < 6; nSlot++) {
        m_characters[nSlot] = CGameObjectArray::INVALID_INDEX;
        m_characterPortraits[nSlot] = CGameObjectArray::INVALID_INDEX;

        if (g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
            m_multiplayerSettings.SetCharacterControlledByPlayer(nSlot, 0, FALSE, FALSE);
            m_multiplayerSettings.SignalCharacterStatus(nSlot,
                CMultiplayerSettings::CHARSTATUS_SIGNAL_DELETED,
                TRUE,
                FALSE);
            m_multiplayerSettings.SetCharacterReady(nSlot, FALSE, FALSE);
        }
    }

    if (g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
        g_pBaldurChitin->GetBaldurMessage()->SendFullSettingsToClients(CString(""));
    }
}

// 0x5BD9D0
BOOL CInfGame::AddCharacterToAllies(LONG nCharacterId)
{
    // NOTE: Uninline.
    if (GetCharacterPortraitNum(nCharacterId) != -1) {
        return FALSE;
    }

    if (m_allies.Find(reinterpret_cast<int*>(nCharacterId)) != NULL) {
        return FALSE;
    }

    if (m_familiars.Find(reinterpret_cast<int*>(nCharacterId)) != NULL) {
        return FALSE;
    }

    m_allies.AddTail(reinterpret_cast<int*>(nCharacterId));

    return TRUE;
}

// 0x5BDA50
BOOL CInfGame::RemoveCharacterFromAllies(LONG nCharacterId)
{
    POSITION pos = m_allies.Find(reinterpret_cast<int*>(nCharacterId));
    if (pos == NULL) {
        return FALSE;
    }

    m_allies.RemoveAt(pos);

    // FIXME: What for?
    if (m_allies.GetCount() == 0) {
        m_allies.RemoveAll();
    }

    return TRUE;
}

// 0x5BDAA0
BOOL CInfGame::AddCharacterToFamiliars(LONG nCharacterId)
{
    // NOTE: Uninline.
    if (GetCharacterPortraitNum(nCharacterId) != -1) {
        return FALSE;
    }

    if (m_allies.Find(reinterpret_cast<int*>(nCharacterId)) != NULL) {
        return FALSE;
    }

    if (m_familiars.Find(reinterpret_cast<int*>(nCharacterId)) != NULL) {
        return FALSE;
    }

    m_familiars.AddTail(reinterpret_cast<int*>(nCharacterId));

    return TRUE;
}

// 0x5BDB20
BOOL CInfGame::RemoveCharacterFromFamiliars(LONG nCharacterId)
{
    POSITION pos = m_familiars.Find(reinterpret_cast<int*>(nCharacterId));
    if (pos == NULL) {
        return FALSE;
    }

    m_familiars.RemoveAt(pos);

    // FIXME: What for?
    if (m_familiars.GetCount() == 0) {
        m_familiars.RemoveAll();
    }

    return TRUE;
}

// 0x5BDB70
DWORD CInfGame::GetScrollSpeed()
{
    if (m_gameAreas[m_visibleArea]->m_nKeyScrollState != 0) {
        return m_cOptions.m_nKeyboardScrollSpeed;
    } else {
        return m_cOptions.m_nMouseScrollSpeed;
    }
}

// 0x5BDBA0
void CInfGame::sub_5BDBA0(int a1, BOOL a2) // re-lint: disable RE004 (address-named in this tree; rename is Ghidra-first)
{
    // Sets the reputation from whole points (1..20); stored x10. The clamps
    // run only when told to, matching the three separate stores in the exe.
    m_nReputation = static_cast<SHORT>(a1 * 10);
    if (a2 != FALSE) {
        if (m_nReputation > 200) {
            m_nReputation = 200;
        }
        if (m_nReputation < 10) {
            m_nReputation = 10;
        }
    }
}

// 0x5BDC00
void CInfGame::ReputationAdjustment(SHORT reputationAdjustment, BOOL set)
{
    if (set != FALSE) {
        // "Set the reputation to N" travels as the delta to N.
        reputationAdjustment -= m_nReputation;
    }

    // The change reaches the other players as a message whose Run applies it
    // here; while that runs, this call must not post another one back.
    if (g_pBaldurChitin->GetBaldurMessage()->m_bInReputationChange == FALSE) {
        CMessage* message = new CMessageReputationChange(reputationAdjustment,
            GetProtagonist(),
            GetProtagonist());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    m_nReputation += reputationAdjustment;
    // 1..20 in game terms, stored x10.
    if (m_nReputation > 200) {
        m_nReputation = 200;
    }
    if (m_nReputation < 10) {
        m_nReputation = 10;
    }

    // Opcode 0x6C, the reputation effect every party member wears.
    ITEM_EFFECT itemEffect;
    CGameEffect::ClearItemEffect(&itemEffect, CGAMEEFFECT_REPUTATION);
    itemEffect.effectAmount = m_nReputation;
    itemEffect.dwFlags = 1;
    // NOTE: The duration type is the WORD constant at 0x8491AA (1).
    itemEffect.durationType = 1;

    for (SHORT nPortrait = 0; nPortrait < m_nCharacters; nPortrait++) {
        LONG id = GetCharacterId(nPortrait);
        CGameSprite* pSprite;
        BYTE rc;
        CMessage* message;

        do {
            rc = m_cObjectArray.GetDeny(id,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            continue;
        }

        if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
            || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID) {
            // HAPPY.2DA: the row is the reputation, the column the member's
            // alignment (good, neutral, evil); anything else is good and an
            // out-of-range cell is the table's default.
            SHORT nColumn = 0;
            BYTE nAlignment = pSprite->GetAIType().m_nAlignment & 0xF;
            if (nAlignment == 2) {
                nColumn = 1;
            } else if (nAlignment == 3) {
                nColumn = 2;
            }
            SHORT nRow = static_cast<SHORT>(m_nReputation / REPUTATION_MULTIPLIER - 1);
            SHORT nHappiness = static_cast<SHORT>(m_ruleTables.m_tHappiness.GetAtLong(CPoint(nColumn, nRow)));

            message = new CMessageSetHappiness(nHappiness, id, id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

            CGameEffect* pEffect = CGameEffect::DecodeEffect(&itemEffect,
                CPoint(-1, -1),
                -1,
                CPoint(-1, -1));
            message = new CMessageAddEffect(pEffect, id, id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

            pSprite->CheckReputation();
        }

        m_cObjectArray.ReleaseDeny(id,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    if (g_pChitin->cNetwork.GetSessionOpen() == FALSE
        || g_pChitin->cNetwork.GetSessionHosting() == TRUE) {
        FeedBack(FEEDBACK_REPUTATION, reputationAdjustment / REPUTATION_MULTIPLIER, TRUE);
    }
}

// 0x5BDF80
void CInfGame::ChangeReputation(SHORT eventCode)
{
    LONG repMod = atol(m_ruleTables.m_tReputationMod.GetAt(CPoint(eventCode, m_nReputation / REPUTATION_MULTIPLIER - 1)));
    ReputationAdjustment(static_cast<SHORT>(repMod), FALSE);
}

// The party member a Decode object picks (Decode 0x40C445): 1 the lowest
// level, 2 the highest, 3 the most damaged, 4 the least damaged, 5 the
// highest-level male, 6 the worst AC, 7 the best AC. Dead members are
// skipped; a level tie goes to the class mask level (0xD1). INVALID_INDEX
// when nobody qualifies.
//
// 0x5BDFE0
LONG CInfGame::sub_5BDFE0(SHORT nType)
{
    LONG nBest = CGameObjectArray::INVALID_INDEX;
    INT nBestClassLevel = 0;
    // NOTE: Case 5 compares a level tie with a stack slot it never sets
    // (local_4, 0x5BE35E); ours starts it at 0.
    INT nUnsetClassLevel = 0;
    SHORT nIndex;
    SHORT nValue;
    CGameSprite* pSprite;
    BYTE rc;

    switch (nType) {
    case 1:
        nValue = 0x7FFF;
        for (nIndex = 0; nIndex < m_nCharacters; nIndex++) {
            do {
                rc = m_cObjectArray.GetShare(GetCharacterId(nIndex), CGameObjectArray::THREAD_ASYNCH, reinterpret_cast<CGameObject**>(&pSprite), INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);
            if (rc != CGameObjectArray::SUCCESS) {
                continue;
            }
            if ((pSprite->GetDerivedStats()->m_generalState & STATE_DEAD) == 0) {
                BYTE nLevel = pSprite->GetDerivedStats()->m_nLevel;
                if (nLevel < nValue) {
                    nBest = GetCharacterId(nIndex);
                    nBestClassLevel = pSprite->GetDerivedStats()->GetClassMaskLevel(0xD1);
                    nValue = nLevel;
                } else if (nLevel == nValue) {
                    INT nClassLevel = pSprite->GetDerivedStats()->GetClassMaskLevel(0xD1);
                    if (nBestClassLevel > nClassLevel) {
                        nBest = GetCharacterId(nIndex);
                        nValue = pSprite->GetDerivedStats()->m_nLevel;
                        nBestClassLevel = nClassLevel;
                    }
                }
            }
            m_cObjectArray.ReleaseShare(GetCharacterId(nIndex), CGameObjectArray::THREAD_ASYNCH, INFINITE);
        }
        return nBest;
    case 2:
    case 5:
        nValue = 0;
        for (nIndex = 0; nIndex < m_nCharacters; nIndex++) {
            do {
                rc = m_cObjectArray.GetShare(GetCharacterId(nIndex), CGameObjectArray::THREAD_ASYNCH, reinterpret_cast<CGameObject**>(&pSprite), INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);
            if (rc != CGameObjectArray::SUCCESS) {
                continue;
            }
            if ((pSprite->GetDerivedStats()->m_generalState & STATE_DEAD) == 0
                && (nType == 2 || pSprite->GetAIType().m_nGender == 1)) {
                BYTE nLevel = pSprite->GetDerivedStats()->m_nLevel;
                if (nLevel > nValue) {
                    nBest = GetCharacterId(nIndex);
                    if (nType == 2) {
                        nBestClassLevel = pSprite->GetDerivedStats()->GetClassMaskLevel(0xD1);
                    }
                    nValue = nLevel;
                } else if (nLevel == nValue) {
                    INT nClassLevel = pSprite->GetDerivedStats()->GetClassMaskLevel(0xD1);
                    if ((nType == 2 ? nBestClassLevel : nUnsetClassLevel) < nClassLevel) {
                        nBest = GetCharacterId(nIndex);
                        nValue = pSprite->GetDerivedStats()->m_nLevel;
                        if (nType == 2) {
                            nBestClassLevel = nClassLevel;
                        }
                    }
                }
            }
            m_cObjectArray.ReleaseShare(GetCharacterId(nIndex), CGameObjectArray::THREAD_ASYNCH, INFINITE);
        }
        return nBest;
    case 3:
    case 4:
        nValue = nType == 3 ? 0 : 0x7FFF;
        for (nIndex = 0; nIndex < m_nCharacters; nIndex++) {
            do {
                rc = m_cObjectArray.GetShare(GetCharacterId(nIndex), CGameObjectArray::THREAD_ASYNCH, reinterpret_cast<CGameObject**>(&pSprite), INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);
            if (rc != CGameObjectArray::SUCCESS) {
                continue;
            }
            if ((pSprite->GetDerivedStats()->m_generalState & STATE_DEAD) == 0) {
                SHORT nDamage = pSprite->GetDerivedStats()->m_nMaxHitPoints - pSprite->GetBaseStats()->m_hitPoints;
                if (nType == 3 ? nValue < nDamage : nDamage < nValue) {
                    nBest = GetCharacterId(nIndex);
                    nValue = nDamage;
                }
            }
            m_cObjectArray.ReleaseShare(GetCharacterId(nIndex), CGameObjectArray::THREAD_ASYNCH, INFINITE);
        }
        return nBest;
    case 6:
    case 7:
        nValue = nType == 6 ? -20 : 20;
        for (nIndex = 0; nIndex < m_nCharacters; nIndex++) {
            do {
                rc = m_cObjectArray.GetShare(GetCharacterId(nIndex), CGameObjectArray::THREAD_ASYNCH, reinterpret_cast<CGameObject**>(&pSprite), INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);
            if (rc != CGameObjectArray::SUCCESS) {
                continue;
            }
            if ((pSprite->GetDerivedStats()->m_generalState & STATE_DEAD) == 0) {
                INT nAC = pSprite->GetAC();
                if (nType == 6 ? nValue < nAC : nAC < nValue) {
                    nBest = GetCharacterId(nIndex);
                    nValue = static_cast<SHORT>(pSprite->GetAC());
                }
            }
            m_cObjectArray.ReleaseShare(GetCharacterId(nIndex), CGameObjectArray::THREAD_ASYNCH, INFINITE);
        }
        return nBest;
    }

    return CGameObjectArray::INVALID_INDEX;
}

// 0x5BF4B0
void CInfGame::AddDisposableItem(CItem* pItem)
{
    if (pItem != NULL) {
        CSingleLock disposableItemsLock(&m_disposableItemsCritSect, FALSE);
        disposableItemsLock.Lock(INFINITE);

        m_lDisposableItems.AddTail(pItem);

        disposableItemsLock.Unlock();
    }
}

// 0x5BF540
void CInfGame::DestroyDisposableItems()
{
    if (m_lDisposableItems.GetCount() > 0) {
        CSingleLock disposableItemsLock(&m_disposableItemsCritSect, FALSE);
        disposableItemsLock.Lock(INFINITE);

        POSITION pos = m_lDisposableItems.GetHeadPosition();
        while (pos != NULL) {
            CItem* pItem = m_lDisposableItems.GetAt(pos);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 16931
            UTIL_ASSERT(pItem != NULL);

            delete pItem;

            // NOTE: Meaningless.
            m_lDisposableItems.SetAt(pos, NULL);

            m_lDisposableItems.GetNext(pos);
        }

        m_lDisposableItems.RemoveAll();

        disposableItemsLock.Unlock();
    }
}

// 0x5BF610
void CInfGame::AddPartyGold(LONG dwAddPartyGold)
{
    if (dwAddPartyGold != 0) {
        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
            CMessagePartyGold* pMessage = new CMessagePartyGold(FALSE,
                TRUE,
                dwAddPartyGold,
                CGameObjectArray::INVALID_INDEX,
                CGameObjectArray::INVALID_INDEX);

            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
        } else {
            if (dwAddPartyGold >= 0 || static_cast<DWORD>(-dwAddPartyGold) <= m_gameSave.m_nPartyGold) {
                m_gameSave.m_nPartyGold += dwAddPartyGold;
            } else {
                m_gameSave.m_nPartyGold = 0;
            }
        }
    }
}

// 0x5BF6A0
void CInfGame::SetupCharacters(BOOLEAN bProgressBarInPlace)
{
    CNetwork* pNetwork = &(g_pChitin->cNetwork);
    CResRef cResArea;
    CString sArea;
    CPoint ptView;
    CGameArea* pArea;
    CGameSprite* pSprite;
    LONG nCharacterId;
    SHORT nCharacterSlot;
    CPoint ptStart;
    int nStartRotation;

    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();
    ruleTables.GetStartArea(cResArea, ptView);
    cResArea.CopyToString(sArea);

    if (m_bFromNewGame) {
        pArea = LoadArea(sArea, 255, TRUE, bProgressBarInPlace);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 17025
        UTIL_ASSERT(pArea != NULL);

        pArea->m_cInfinity.SetViewPosition(ptView.x - (pArea->m_cInfinity.rViewPort.right - pArea->m_cInfinity.rViewPort.left) / 2,
            ptView.y - (pArea->m_cInfinity.rViewPort.bottom - pArea->m_cInfinity.rViewPort.top) / 2,
            TRUE);
    } else {
        pArea = LoadArea(m_multiplayerSettings.GetAreaStartName(), 255, FALSE, bProgressBarInPlace);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 17032
        UTIL_ASSERT(pArea != NULL);
    }

    for (nCharacterSlot = 0; nCharacterSlot < 6; nCharacterSlot++) {
        nCharacterId = nCharacterSlot < 6
            ? m_characters[nCharacterSlot]
            : CGameObjectArray::INVALID_INDEX;

        if (pNetwork->m_bIsHost
            && m_multiplayerSettings.GetCharacterStatus(nCharacterSlot) != CMultiplayerSettings::CHARSTATUS_CHARACTER) {
            continue;
        }

        if (nCharacterId == CGameObjectArray::INVALID_INDEX) {
            continue;
        }

        if (m_cObjectArray.GetDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE)
            != CGameObjectArray::SUCCESS) {
            continue;
        }

        if (nCharacterSlot == 0 && m_nReputation / REPUTATION_MULTIPLIER < 0) {
            m_nReputation = ruleTables.GetStartingReputation(pSprite->m_startTypeAI.m_nAlignment) * 10;
            m_nReputation = m_nReputation > 200 ? 200 : m_nReputation;
            m_nReputation = m_nReputation < 10 ? 10 : m_nReputation;
        }

        pSprite->m_baseStats.m_gold = 0;

        if (pSprite->GetPos().x == -1 && pSprite->GetPos().y == -1) {
            if (m_bFromNewGame) {
                ptStart = ruleTables.GetStartPoint(nCharacterSlot);
                nStartRotation = ruleTables.GetStartRotation(nCharacterSlot);
            } else if (pNetwork->m_bConnectionEstablished) {
                BYTE nPersonalSpace = pSprite->m_animation.GetPersonalSpace();
                CPoint ptOpen = pArea->m_search.GetNearestOpenSquare(CPoint(m_multiplayerSettings.m_ptAreaStart.x / CPathSearch::GRID_SQUARE_SIZEX,
                                                                         m_multiplayerSettings.m_ptAreaStart.y / CPathSearch::GRID_SQUARE_SIZEY),
                    pSprite->GetTerrainTable(),
                    nPersonalSpace,
                    -1);
                ptStart.x = ptOpen.x * CPathSearch::GRID_SQUARE_SIZEX;
                ptStart.y = ptOpen.y * CPathSearch::GRID_SQUARE_SIZEY;
            } else {
                ptStart.x = -1;
                ptStart.y = -1;
            }

            m_bInLoadGame = TRUE;

            BOOL bMusicThreadPriorityChanged = FALSE;
            int nMusicThreadPriority = GetThreadPriority(g_pChitin->m_hMusicThread);

            if (GetPrivateProfileIntA("Program Options", "Volume Music", 0, g_pChitin->GetIniFileName())) {
                bMusicThreadPriorityChanged = SetThreadPriority(g_pChitin->m_hMusicThread, 15);
            }

            if (pSprite->GetArea() != NULL) {
                pSprite->RemoveFromArea();
            }

            if ((pSprite->m_baseStats.m_generalState & STATE_DEAD) != 0
                && pSprite->GetAnimation()->CanLieDown()) {
                pSprite->AddToArea(pArea, ptStart, 0, CGameObject::LIST_BACK);
            } else {
                pSprite->AddToArea(pArea, ptStart, 0, CGameObject::LIST_FRONT);
            }

            if (m_bFromNewGame) {
                pSprite->SetFacing(static_cast<SHORT>(nStartRotation));
            }

            m_gameSave.m_cResCurrentWorldArea = pArea->m_resRef;
            m_gameSave.m_nCurrentWorldLink = -1;

            m_bInLoadGame = FALSE;

            if (bMusicThreadPriorityChanged == TRUE) {
                SetThreadPriority(g_pChitin->m_hMusicThread, nMusicThreadPriority);
            }
        }

        m_cObjectArray.ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    if (m_characters[0] != CGameObjectArray::INVALID_INDEX) {
        SetProtagonist(m_characters[0]);
    }
}

// 0x5BFC40
LONG CInfGame::GetProtagonist()
{
    LONG nId;
    SHORT nIndex;
    CGameSprite* pSprite;
    BYTE rc;

    if (!g_pBaldurChitin->m_pEngineWorld->m_internalLoadedDialog.field_54
        || (nId = g_pBaldurChitin->m_pEngineWorld->m_internalLoadedDialog.m_characterIndex) == 0) {
        nIndex = 0;
        do {
            nId = nIndex < m_nCharacters
                ? m_characterPortraits[nIndex]
                : CGameObjectArray::INVALID_INDEX;

            do {
                rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                if (pSprite->m_bSelected && !IcewindMisc::IsDead(pSprite)) {
                    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                    if (nId != 0) {
                        goto found;
                    }
                    break;
                }

                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        } while (nIndex++ < m_nCharacters);

        nIndex = 0;
        do {
            nId = nIndex < 6
                ? m_characters[nIndex]
                : CGameObjectArray::INVALID_INDEX;

            do {
                rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                if (!IcewindMisc::IsDead(pSprite)) {
                    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                    if (nId != 0) {
                        goto found;
                    }
                    break;
                }

                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        } while (nIndex++ < m_nCharacters);

        nId = m_characters[0];
    }

found:
    CMessageSetProtagonist* pMessage = new CMessageSetProtagonist(nId, nId, nId);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

    return nId;
}

// 0x5BFE10
void CInfGame::SetProtagonist(LONG nId)
{
    STR_RES strRes;

    if (g_pBaldurChitin->m_pEngineWorld->m_internalLoadedDialog.field_54) {
        nId = g_pBaldurChitin->m_pEngineWorld->m_internalLoadedDialog.m_characterIndex;
    }

    STRREF strSurfaceUnderdark = 25688; // "sun-lander"

    if (nId == -1) {
        return;
    }

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(g_pBaldurChitin->GetObjectGame()->m_characters[0],
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    // NOTE: Original code is slightly different. It sets token only if
    // `GetName` returns non-NULL, but this implementation already returns
    // reference.
    g_pBaldurChitin->GetTlkTable().SetToken(TOKEN_CHARACTER, pSprite->GetName());

    STRREF strRace;
    switch (pSprite->GetAIType().m_nRace) {
    case CAIOBJECTTYPE_R_HUMAN:
        strRace = 7193;
        break;
    case CAIOBJECTTYPE_R_ELF:
        strRace = 7194;
        break;
    case CAIOBJECTTYPE_R_HALF_ELF:
        strRace = 7197;
        break;
    case CAIOBJECTTYPE_R_DWARF:
        strRace = 7182;
        if (pSprite->GetAIType().m_nSubRace == 2
            || pSprite->GetAIType().m_nSubRace == 1) {
            strSurfaceUnderdark = 25689; // "outsider"
        }
        break;
    case CAIOBJECTTYPE_R_HALFLING:
        strRace = 7195;
        break;
    case CAIOBJECTTYPE_R_GNOME:
        strRace = 7196;
        if (pSprite->GetAIType().m_nSubRace == 1) {
            strSurfaceUnderdark = 25689; // "outsider"
        }
        break;
    case CAIOBJECTTYPE_R_HALF_ORC:
        strRace = 22;
        break;
    default:
        strRace = -1;
        break;
    }

    g_pBaldurChitin->GetTlkTable().Fetch(strRace, strRes);
    g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_RACE, strRes.szText);

    g_pBaldurChitin->GetTlkTable().Fetch(strSurfaceUnderdark, strRes);
    g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_SURFACE_UNDERDARK, strRes.szText);

    if (pSprite->GetAIType().m_nGender == CAIObjectType::SEX_FEMALE) {
        g_pBaldurChitin->GetTlkTable().Fetch(27475, strRes); // "ma'am"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_SIRMAAM, strRes.szText);

        g_pBaldurChitin->GetTlkTable().Fetch(27476, strRes); // "girl"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_GIRLBOY, strRes.szText);

        g_pBaldurChitin->GetTlkTable().Fetch(27479, strRes); // "sister"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_BROTHERSISTER, strRes.szText);

        g_pBaldurChitin->GetTlkTable().Fetch(27480, strRes); // "lady"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_LADYLORD, strRes.szText);

        g_pBaldurChitin->GetTlkTable().Fetch(27483, strRes); // "female"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_MALEFEMALE, strRes.szText);

        g_pBaldurChitin->GetTlkTable().Fetch(27485, strRes); // "she"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_HESHE, strRes.szText);

        g_pBaldurChitin->GetTlkTable().Fetch(27487, strRes); // "her"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_HISHER, strRes.szText);

        g_pBaldurChitin->GetTlkTable().Fetch(27487, strRes); // "her"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_HIMHER, strRes.szText);

        g_pBaldurChitin->GetTlkTable().Fetch(27490, strRes); // "woman"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_MANWOMAN, strRes.szText);
    } else {
        g_pBaldurChitin->GetTlkTable().Fetch(27473, strRes); // "sir"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_SIRMAAM, strRes.szText);

        g_pBaldurChitin->GetTlkTable().Fetch(27477, strRes); // "boy"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_GIRLBOY, strRes.szText);

        g_pBaldurChitin->GetTlkTable().Fetch(27478, strRes); // "brother"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_BROTHERSISTER, strRes.szText);

        g_pBaldurChitin->GetTlkTable().Fetch(27481, strRes); // "lord"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_LADYLORD, strRes.szText);

        g_pBaldurChitin->GetTlkTable().Fetch(27482, strRes); // "male"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_MALEFEMALE, strRes.szText);

        g_pBaldurChitin->GetTlkTable().Fetch(27484, strRes); // "he"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_HESHE, strRes.szText);

        g_pBaldurChitin->GetTlkTable().Fetch(27486, strRes); // "his"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_HISHER, strRes.szText);

        g_pBaldurChitin->GetTlkTable().Fetch(27488, strRes); // "him"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_HIMHER, strRes.szText);

        g_pBaldurChitin->GetTlkTable().Fetch(27489, strRes); // "man"
        g_pBaldurChitin->GetTlkTable().SetToken(CInfGame::TOKEN_MANWOMAN, strRes.szText);
    }

    m_nProtagonistId = nId;

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(g_pBaldurChitin->GetObjectGame()->m_characters[0],
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// 0x5C04F0
BOOLEAN CInfGame::FindAreaID(DWORD nAreaId)
{
    for (int index = 0; index < CINFGAME_MAX_AREAS; index++) {
        if (m_gameAreas[index] != NULL) {
            if (m_gameAreas[index]->m_nInitialAreaID == nAreaId) {
                return TRUE;
            }
        }
    }

    return FALSE;
}

// 0x5C09C0
CString CInfGame::GetDirSaveRoot()
{
    CString sResolvedFileName;
    CString sFileName;

    sFileName = m_sMultiplayerSaveDir;
    if (!g_pBaldurChitin->lAliases.ResolveFileName(sFileName, sResolvedFileName)) {
        sResolvedFileName = sFileName;
    }

    return sResolvedFileName;
}

// 0x5C0A80
CString CInfGame::GetDirSave()
{
    return m_sMultiplayerSaveDir + m_sSaveGame + "\\";
}

// TODO: Somehow its different from function above. Figure out.
//
// 0x5C0B30
CString CInfGame::sub_5C0B30()
{
    return m_sMultiplayerSaveDir + m_sSaveGame + "\\";
}

// 0x5C20E0
BOOL CInfGame::CanEnterStore(STRREF& strError)
{
    BOOL bScattered = FALSE;
    CResRef cResArea;

    for (SHORT nPortrait = 0; nPortrait < m_nCharacters; nPortrait++) {
        if (bScattered) {
            break;
        }

        LONG nCharacterId = GetCharacterId(nPortrait);

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = m_cObjectArray.GetDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            if (pSprite->Animate()) {
                if (cResArea != "") {
                    if (cResArea != pSprite->GetArea()->m_resRef) {
                        bScattered = TRUE;
                    }
                } else {
                    cResArea = pSprite->GetArea()->m_resRef;
                }
            }

            m_cObjectArray.ReleaseDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        } else {
            bScattered = TRUE;
        }
    }

    if (!CanSaveGame(strError, TRUE, TRUE)) {
        if (strError == 16501) {
            // "You cannot enter the store; there are monsters nearby."
            strError = 20049;
        } else if (strError == 16502) {
            // "You may not enter the store at this time."
            strError = 261;
        } else {
            // "Not permitted to buy/sell items."
            strError = 10847;
        }
        return FALSE;
    }

    if (bScattered) {
        // "You may not enter the store when your party is scattered."
        strError = 20690;
        return FALSE;
    }

    return TRUE;
}

// 0x5C2250
BOOL CInfGame::AddCharacterToOverflow(LONG id)
{
    m_characterOverflow.Add(reinterpret_cast<int*>(id));
    m_nCharacterOverflowCount++;
    return TRUE;
}

// 0x5C2280
CStringList* CInfGame::GetScripts()
{
    CStringList* pList = new CStringList();

    CString sFileName;
    CString sPattern;
    CString sTemp;

    sPattern = GetDirScripts() + "*.*";

    INT nCount = 0;
    WIN32_FIND_DATAA findFileData;
    HANDLE hFindFile = FindFirstFileA(sPattern, &findFileData);
    if (hFindFile != INVALID_HANDLE_VALUE) {
        do {
            sFileName = findFileData.cFileName;
            if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0
                && sFileName != "."
                && sFileName != "..") {
                sFileName = sFileName.SpanExcluding(".");
                if (sFileName.GetLength() <= 8) {
                    if (nCount >= 16383) {
                        break;
                    }

                    sFileName.MakeUpper();

                    POSITION pos = pList->GetHeadPosition();
                    while (pos != NULL) {
                        sTemp = pList->GetAt(pos);
                        if (sFileName == sTemp) {
                            break;
                        }

                        if (sFileName < pList->GetAt(pos)) {
                            pList->InsertBefore(pos, sFileName);
                            nCount++;
                            break;
                        }

                        pList->GetNext(pos);
                    }

                    if (pos == NULL) {
                        pList->AddTail(sFileName);
                        nCount++;
                    }
                }
            }
        } while (FindNextFileA(hFindFile, &findFileData));
        FindClose(hFindFile);
    }

    return pList;
}

// 0x5C24C0
CString CInfGame::GetDirScripts()
{
    return m_sScriptsDir;
}

// 0x5C24F0
void CInfGame::StartCharacterTerminationSequence()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->m_gameSave.m_mode == 322) {
        pGame->m_gameSave.m_mode = -1;
    }

    pGame->m_gameSave.m_cutScene = FALSE;

    if (g_pBaldurChitin->GetActiveEngine() != g_pBaldurChitin->m_pEngineWorld) {
        g_pBaldurChitin->GetActiveEngine()->SelectEngine(g_pBaldurChitin->m_pEngineWorld);
    }

    g_pBaldurChitin->m_pEngineWorld->m_bGameOverPanel = TRUE;
}

// 0x5C2570
void CInfGame::ReadyCharacterTerminationSequence(int a1, int a2)
{
    if (!m_gameSave.field_1AC) {
        // FIXME: What for (this function is not static as
        // `StartCharacterTerminationSequence`).
        CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

        pGame->m_gameSave.m_mode = 322;
        pGame->m_gameSave.m_cutScene = TRUE;

        g_pBaldurChitin->GetObjectCursor()->SetCursor(0, FALSE);

        pGame->m_tempCursor = 4;
        pGame->m_gameAreas[pGame->m_visibleArea]->m_nScrollState = 0;
        pGame->m_nCharacterTerminationSequenceDelay = 150;

        g_pBaldurChitin->m_pEngineWorld->field_10F0 = a1;
        g_pBaldurChitin->m_pEngineWorld->field_10F4 = a2;
    }
}

// 0x5C0BB0
//
// Shares nExperience among the living party members (STATE_DEAD clear), as
// an XP effect (104) each: the remainder of the division goes one point at
// a time to the first ones, then the multiclass penalty applies. With
// bChallengeRating the amount is first looked up against the members'
// average level (CRuleTables::sub_542560) and halved. Returns the total.
LONG CInfGame::AddExperienceParty(DWORD nExperience, BOOLEAN a2, BOOLEAN bChallengeRating)
{
    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();
    CGameSprite* pSprite;
    BYTE rc;
    SHORT nAlive = 0;
    SHORT nLevels = 0;

    for (SHORT nIndex = 0; nIndex < m_nCharacters; nIndex++) {
        LONG nId = nIndex < m_nCharacters ? m_characterPortraits[nIndex] : CGameObjectArray::INVALID_INDEX;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            if ((pSprite->m_derivedStats.m_generalState & STATE_DEAD) == 0) {
                nAlive++;
                nLevels += pSprite->m_derivedStats.m_nLevel;
            }

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    if (nAlive == 0) {
        nAlive = 1;
    }

    DWORD nTotal = nExperience;
    if (bChallengeRating) {
        nTotal = static_cast<DWORD>(ruleTables.sub_542560(static_cast<unsigned short>(nExperience),
                     static_cast<unsigned short>(nLevels / nAlive)))
            >> 1;
    }

    const DWORD nShare = nTotal / static_cast<DWORD>(nAlive);
    SHORT nRemainder = static_cast<SHORT>(nTotal % static_cast<DWORD>(nAlive));

    ITEM_EFFECT effect;
    CGameEffect::ClearItemEffect(&effect, 104);
    // NOTE: The duration type is the WORD constant at 0x8491AA (1).
    effect.durationType = 1;
    effect.effectAmount = nShare;

    LONG nGiven = 0;
    for (SHORT nIndex = 0; nIndex < m_nCharacters; nIndex++) {
        LONG nId = nIndex < m_nCharacters ? m_characterPortraits[nIndex] : CGameObjectArray::INVALID_INDEX;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            continue;
        }

        if ((pSprite->m_derivedStats.m_generalState & STATE_DEAD) == 0) {
            DWORD nAmount = nShare;
            if (nRemainder > 0) {
                nAmount = nShare + 1;
                nRemainder--;
            }
            effect.effectAmount = nAmount;

            INT nPenalty = pSprite->GetMulticlassingPenalty();
            if (nPenalty > 0) {
                effect.effectAmount = (100 - nPenalty) * effect.effectAmount / 100;
            }

            nGiven += effect.effectAmount;

            CGameEffect* pEffect = CGameEffect::DecodeEffect(&effect, CPoint(-1, -1), -1, CPoint(-1, -1));
            LONG nTarget = nIndex < m_nCharacters ? m_characterPortraits[nIndex] : CGameObjectArray::INVALID_INDEX;
            CMessage* message = new CMessageAddEffect(pEffect, nTarget, nTarget);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    FeedBack(2, nGiven, a2);

    return nGiven;
}

// 0x5C2630
void CInfGame::FeedBack(SHORT feedBackId, LONG value, BOOLEAN a3)
{
    CString sTime;

    if (!a3) {
        a3 = m_cOptions.m_bShowQuestXP;
    }

    switch (feedBackId) {
    case FEEDBACK_GOLD:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            CString sText;
            STR_RES strRes;

            if (value != 0) {
                if (value > 0) {
                    // "The party has gained gold"
                    g_pBaldurChitin->GetTlkTable().Fetch(17572, strRes);
                } else {
                    // "The party has lost gold"
                    g_pBaldurChitin->GetTlkTable().Fetch(17573, strRes);
                    value = -value;
                }
                sText.Format("%d", value);

                if (strRes.cSound.GetRes() != NULL) {
                    if (!strRes.cSound.GetLooping()) {
                        strRes.cSound.SetFireForget(TRUE);
                    }
                    strRes.cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(GetVisibleArea()));
                    strRes.cSound.Play(FALSE);
                }

                if (strRes.szText != "") {
                    if (a3) {
                        sText = strRes.szText + ": " + sText;
                    } else {
                        sText = strRes.szText + ".";
                    }
                }

                g_pBaldurChitin->GetBaldurMessage()->DisplayText(CString(""),
                    sText,
                    RGB(0, 0, 0),
                    RGB(255, 255, 0),
                    -1,
                    0,
                    0);
            }
        }
        break;
    case FEEDBACK_EXPERIENCE:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            CString sText;
            STR_RES strRes;

            if (value != 0) {
                if (value > 0) {
                    // "The party has gained experience"
                    g_pBaldurChitin->GetTlkTable().Fetch(17574, strRes);
                } else {
                    // "The party has lost experience"
                    g_pBaldurChitin->GetTlkTable().Fetch(17575, strRes);
                    value = -value;
                }
                sText.Format("%d", value);

                if (strRes.cSound.GetRes() != NULL) {
                    if (!strRes.cSound.GetLooping()) {
                        strRes.cSound.SetFireForget(TRUE);
                    }
                    strRes.cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(GetVisibleArea()));
                    strRes.cSound.Play(FALSE);
                }

                if (strRes.szText != "") {
                    if (a3) {
                        sText = strRes.szText + ": " + sText;
                    } else {
                        sText = strRes.szText + ".";
                    }
                }

                g_pBaldurChitin->GetBaldurMessage()->DisplayText(CString(""),
                    sText,
                    RGB(0, 0, 0),
                    RGB(255, 255, 0),
                    -1,
                    0,
                    0);
            }
        }
        break;
    case FEEDBACK_ITEMGAINED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            // "The party has gained an item."
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(-1,
                17576,
                RGB(0, 0, 0),
                RGB(255, 255, 0),
                -1,
                0,
                0);
        }
        break;
    case FEEDBACK_ITEMLOST:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            // "The party has lost an item."
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(-1,
                17577,
                RGB(0, 0, 0),
                RGB(255, 255, 0),
                -1,
                0,
                0);
        }
        break;
    case FEEDBACK_REPUTATION:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            CString sText;
            STR_RES strRes;

            if (value != 0) {
                if (value > 0) {
                    // "The party's reputation has increased"
                    g_pBaldurChitin->GetTlkTable().Fetch(19686, strRes);
                } else {
                    // "The party's reputation has decreased"
                    g_pBaldurChitin->GetTlkTable().Fetch(19687, strRes);
                    value = -value;
                }
                sText.Format("%d", value);

                if (strRes.cSound.GetRes() != NULL) {
                    if (!strRes.cSound.GetLooping()) {
                        strRes.cSound.SetFireForget(TRUE);
                    }
                    strRes.cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(GetVisibleArea()));
                    strRes.cSound.Play(FALSE);
                }

                if (strRes.szText != "") {
                    if (a3) {
                        sText = strRes.szText + ": " + sText;
                    } else {
                        sText = strRes.szText + ".";
                    }
                }

                g_pBaldurChitin->GetBaldurMessage()->DisplayText(CString(""),
                    sText,
                    RGB(0, 0, 0),
                    RGB(255, 255, 0),
                    -1,
                    0,
                    0);
            }
        }
        break;
    case FEEDBACK_TIMEPASSEDJOURNEY:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            // NOTE: Signed compare.
            if (value >= static_cast<LONG>(CTimerWorld::TIMESCALE_MSEC_PER_HOUR)) {
                // "The journey took <DURATION>."
                CTimerWorld::GetCurrentTimeString(value, 10689, sTime);
                g_pBaldurChitin->GetBaldurMessage()->DisplayText(CString(""),
                    sTime,
                    RGB(0, 0, 0),
                    RGB(255, 255, 0),
                    -1,
                    0,
                    0);
            }
        }
        break;
    case FEEDBACK_TIMEPASSEDREST:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            // NOTE: Signed compare.
            if (value >= static_cast<LONG>(CTimerWorld::TIMESCALE_MSEC_PER_HOUR)) {
                // "You have rested for <DURATION>."
                CTimerWorld::GetCurrentTimeString(value, 10690, sTime);
                g_pBaldurChitin->GetBaldurMessage()->DisplayText(CString(""),
                    sTime,
                    RGB(0, 0, 0),
                    RGB(255, 255, 0),
                    -1,
                    0,
                    0);
            }
        }
        break;
    case FEEDBACK_INVENTORYFULL_ITEMDROPPED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            // "Inventory Full: The item has been dropped."
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(-1,
                10959,
                RGB(0, 0, 0),
                RGB(255, 255, 0),
                -1,
                0,
                0);
        }
        break;
    case FEEDBACK_JOURNAL_UPDATE:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            // "Your journal has been updated."
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(-1,
                11359,
                RGB(0, 0, 0),
                RGB(255, 255, 0),
                -1,
                0,
                0);
        }
        break;
    case FEEDBACK_WORLDMAP_UPDATE:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            // "Your map has been updated."
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(-1,
                11360,
                RGB(0, 0, 0),
                RGB(255, 255, 0),
                -1,
                0,
                0);
        }
        break;
    }
}

// 0x5C3210
CString CInfGame::GetDirPortraits()
{
    return m_sPortraitsDir;
}

// 0x5C3510
CString CInfGame::GetDirCharacters()
{
    return m_sCharactersDir;
}

// 0x5C1090
BYTE CInfGame::GetCharactersControlled()
{
    BYTE bControlled = 0;
    CGameObject* pObject;

    for (SHORT nIndex = 0; nIndex < m_nCharacters; nIndex++) {
        LONG id = m_characterPortraits[nIndex];

        BYTE rc;
        do {
            // NOTE: Using global. Implying inlined static function.
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(id,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            // NOTE: Uninline.
            if (pObject->InControl()) {
                bControlled |= 1 << nIndex;
            }

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(id,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    return bControlled;
}

// 0x5C1160
BOOLEAN CInfGame::RestParty(INT nHP, BOOLEAN bRenting)
{
    if (bRenting) {
        for (SHORT nIndex = 0; nIndex < m_nCharacters; nIndex++) {
            // NOTE: Uninline.
            LONG nCharacterId = GetCharacterId(nIndex);

            CMessage101* pMessage = new CMessage101(FALSE,
                nCharacterId,
                nCharacterId,
                TRUE);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
        }
    }

    // A single-player game is a connected host session under the null
    // provider, so the host branch below runs in single player too.
    if (g_pChitin->cNetwork.m_bConnectionEstablished == FALSE) {
        return RestPartyPrivate(nHP, bRenting);
    }

    if (g_pChitin->cNetwork.m_bIsHost == FALSE) {
        if (g_pBaldurChitin->m_cBaldurMessage.m_bInOnRestAnnounce == FALSE) {
            // NOTE: A client asks the server to start the rest here
            // (0x436290 on `m_cBaldurMessage` with nHP, bRenting and
            // `CInfGame::m_bHealPartyOnRest`, 0x5C1265-0x5C1292); it needs a
            // remote server, so it is not ported. The rest happens when the
            // server's announcement arrives.
            return TRUE;
        }

        return RestPartyPrivate(nHP, bRenting);
    }

    if (g_pChitin->cNetwork.m_bIsHost == TRUE) {
        CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;

        // The rest waits while a save is in a dialog mode, the host must
        // permit a dialog, or the world is hard-paused.
        BOOLEAN bStart = TRUE;
        if (m_gameSave.m_mode == 386 || m_gameSave.m_mode == 1282) {
            bStart = FALSE;
        }

        if (m_multiplayerSettings.m_bHostPermittedDialog == TRUE) {
            bStart = FALSE;
        }

        if (pWorld->m_bHardPaused == TRUE || bStart == FALSE) {
            pWorld->SetPendingRest(nHP, bRenting, FALSE);
        } else {
            pWorld->StartRestMultiplayerHost(nHP, bRenting, CInfGame::m_bHealPartyOnRest);
        }

        return TRUE;
    }

    return FALSE;
}

// 0x5C1350
BOOLEAN CInfGame::RestPartyPrivate(INT nHP, BOOLEAN bRenting)
{
    BOOLEAN bResult;
    BOOLEAN bNoneHurt = TRUE;
    INT nHealed;

    CAIAction restAction;
    // NOTE: The SHORT at 0x847834, ACTION.IDS 96 Rest().
    restAction.m_actionID = 96;

    // NOTE: Trigger 0x93 is not in TRIGGER.IDS; only the engine raises it.
    CAITrigger restTrigger(0x93, 0);

    g_pBaldurChitin->GetObjectGame()->sub_5CADF0();

    if (!g_pChitin->cNetwork.m_bConnectionEstablished
        || g_pChitin->cNetwork.m_bIsHost == TRUE) {
        // NOTE: The original looks the chapter variable up and drops the
        // result.
        g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(CHAPTER_GLOBAL);

        // NOTE: The original announces the rest to the clients here
        // (`CBaldurMessage` 0x436500 with nHP, bRenting and 0); a
        // single-player host session calls it too, and its broadcast is a
        // no-op under the null provider. It is not ported.

        nHealed = 0;

        // NOTE: The encounter draws rand() before the rest's own time.
        if (!bRenting
            && m_gameAreas[m_visibleArea]->CheckRestEncounter()) {
            goto done;
        }

        SHORT nCharacters = m_nCharacters;
        if (m_bHealPartyOnRest) {
            for (SHORT nIndex = 0; nIndex < nCharacters; nIndex++) {
                CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
                LONG nCharacterId = pGame->GetCharacterId(nIndex);

                CGameSprite* pSprite;

                BYTE rc;
                do {
                    rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        reinterpret_cast<CGameObject**>(&pSprite),
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                if (rc == CGameObjectArray::SUCCESS) {
                    if ((pSprite->m_baseStats.m_generalState & STATE_DEAD) == 0) {
                        INT nMissing = pSprite->m_baseStats.m_maxHitPointsBase - pSprite->m_baseStats.m_hitPoints;
                        if (nMissing > 1) {
                            bNoneHurt = FALSE;
                        }

                        nHealed += nMissing;

                        INT nLevel = pSprite->m_derivedStats.GetClassMaskLevel(CLASSMASK_CLERIC | CLASSMASK_DRUID);
                        if (nLevel > 0) {
                            nHP += nLevel * 19 + 5;
                        }

                        nLevel = pSprite->m_derivedStats.GetClassMaskLevel(CLASSMASK_PALADIN | CLASSMASK_RANGER);
                        if (nLevel > 9) {
                            nHP += nLevel * 19 - 147;
                        }
                    }

                    pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                }
            }
        } else {
            nHealed = nHP;
        }

        // Round the healing up to a whole number of hours.
        if (nHP > 0) {
            INT nHours = nHealed / nHP;
            if (nHealed % nHP != 0) {
                nHours++;
            }
            nHealed = nHours * nHP;
        }

        if (!m_bHealPartyOnRest) {
            m_worldTime.AddCurrentTime(CRuleTables::TIME_RESTING);
            FeedBack(FEEDBACK_TIMEPASSEDREST, CRuleTables::TIME_RESTING, TRUE);
        } else if (nHealed > nHP) {
            // NOTE: Unsigned arithmetic, as the original's `div`.
            m_worldTime.AddCurrentTime(CRuleTables::TIME_RESTING * nHealed * 100 / (nHP * 100) + CRuleTables::TIME_RESTING);
            FeedBack(FEEDBACK_TIMEPASSEDREST,
                CRuleTables::TIME_RESTING * nHealed * 100 / (nHP * 100) + CRuleTables::TIME_RESTING,
                TRUE);
        } else if (bNoneHurt == TRUE) {
            m_worldTime.AddCurrentTime(CRuleTables::TIME_RESTING);
            FeedBack(FEEDBACK_TIMEPASSEDREST, CRuleTables::TIME_RESTING, TRUE);
        } else {
            m_worldTime.AddCurrentTime(CRuleTables::TIME_RESTING * 2);
            FeedBack(FEEDBACK_TIMEPASSEDREST, CRuleTables::TIME_RESTING * 2, TRUE);
        }

        for (SHORT nIndex = 0; nIndex < nCharacters; nIndex++) {
            // NOTE: Uninline.
            LONG nCharacterId = GetCharacterId(nIndex);

            CGameEffectHeal* pHeal = new CGameEffectHeal();
            if (m_bHealPartyOnRest) {
                pHeal->m_effectAmount = 1000;
            } else {
                pHeal->m_effectAmount = nHealed;
            }

            CMessageAddEffect* pAddEffect = new CMessageAddEffect(pHeal,
                nCharacterId,
                nCharacterId);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pAddEffect, FALSE);

            CMessageAddAction* pAddAction = new CMessageAddAction(restAction,
                nCharacterId,
                nCharacterId);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pAddAction, FALSE);

            CMessageSetTrigger* pSetTrigger = new CMessageSetTrigger(restTrigger,
                nCharacterId,
                nCharacterId);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pSetTrigger, FALSE);
        }

        // NOTE: The DWORD at 0x847F84 (35) and the SHORT at 0x847908,
        // ACTION.IDS 203 FadeFromColor().
        CAIAction fadeAction;
        fadeAction = CAIAction(203, CPoint(35, 0), 0, -1);

        g_pBaldurChitin->GetObjectGame()->GetGroup()->GroupAction(CAIAction(CAIAction::NO_ACTION, CPoint(-1, -1), 0, -1),
            TRUE,
            &fadeAction);
    }

done:
    if (!bRenting) {
        bResult = FALSE;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 18368
        UTIL_ASSERT(g_pBaldurChitin->m_pEngineWorld != NULL);

        if (g_pBaldurChitin->GetActiveEngine() != g_pBaldurChitin->m_pEngineWorld) {
            g_pBaldurChitin->GetActiveEngine()->SelectEngine(g_pBaldurChitin->m_pEngineWorld);
        }
    } else {
        bResult = TRUE;
    }

    // NOTE: The original signals the clients here when a session is open
    // and the party is not renting (`CBaldurMessage::SendSignal` 0x4333C0).
    // Multiplayer is removed (ADR 0002).

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 18481
    UTIL_ASSERT(g_pBaldurChitin->m_pEngineWorld != NULL);

    if (g_pBaldurChitin->GetActiveEngine() != g_pBaldurChitin->m_pEngineWorld) {
        CCacheStatus::dword_8D0BA8 = TRUE;
        g_pBaldurChitin->GetActiveEngine()->SelectEngine(g_pBaldurChitin->m_pEngineWorld);

        // NOTE: A renting client of a multiplayer game tells the server here
        // (0x436C00). Multiplayer is removed (ADR 0002).
    }

    g_pBaldurChitin->GetObjectGame()->m_cButtonArray.ResetState();

    return bResult;
}

// 0x5C1E30
BOOL CInfGame::CanRestParty(STRREF& strError, unsigned char bInStore, unsigned char a3, unsigned char a4)
{
    BOOL bResult;
    BOOL bScattered = FALSE;
    BOOL bAllInDungeon = TRUE;
    BOOL bNoneAlive = TRUE;
    BOOL bRestForbidden = FALSE;
    CResRef cResArea("");
    CPoint ptFirst;

    // "You may not rest at this time."
    strError = 10309;

    bResult = FALSE;

    if (m_nCharacters > 0) {
        for (SHORT nIndex = 0; nIndex < m_nCharacters && !bScattered; nIndex++) {
            // NOTE: Uninline.
            LONG nCharacterId = GetCharacterId(nIndex);

            CGameSprite* pSprite;

            BYTE rc;
            do {
                rc = m_cObjectArray.GetDeny(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc != CGameObjectArray::SUCCESS) {
                bScattered = TRUE;
                continue;
            }

            if (pSprite->GetArea() != NULL
                && (pSprite->GetArea()->m_header.m_flags & 0x2) != 0
                && !bInStore
                && !a4) {
                bRestForbidden = TRUE;
            }

            if (pSprite->Animate()) {
                bNoneAlive = FALSE;

                // NOTE: The original computes this and never reads it.
                if (bAllInDungeon && (pSprite->GetArea()->m_header.m_areaType & 0x20) != 0) {
                    bAllInDungeon = TRUE;
                } else {
                    bAllInDungeon = FALSE;
                }

                CPoint pt = pSprite->GetPos();
                if (cResArea == "") {
                    cResArea = pSprite->GetArea()->m_resRef;
                    ptFirst = pt;
                } else {
                    if (((pSprite->m_baseStats.m_generalState & STATE_DEAD) == 0
                            && cResArea != pSprite->GetArea()->m_resRef)
                        || (ptFirst.y - pt.y) * (ptFirst.y - pt.y) * 16 / 9 + (ptFirst.x - pt.x) * (ptFirst.x - pt.x) > CScreenInventory::RANGE_SWAPITEM) {
                        bScattered = TRUE;
                    }
                }
            }

            m_cObjectArray.ReleaseDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }

        if (bRestForbidden == TRUE) {
            // "You may not rest at this time."
            strError = 10309;
            return FALSE;
        }
    }

    if (a3 == TRUE && !bScattered) {
        return TRUE;
    }

    if (!bNoneAlive) {
        if (!CanSaveGame(strError, TRUE, FALSE)) {
            if (strError == 16501) {
                // "You cannot rest; there are monsters nearby."
                strError = 16500;
                bResult = FALSE;
            } else {
                // "You may not rest at this time."
                strError = 10309;
                bResult = FALSE;
            }
        } else if (bScattered) {
            // "You may not rest when your party is scattered."
            strError = 16457;
            bResult = FALSE;
        } else if (!g_pBaldurChitin->GetObjectGame()->m_singlePlayerPermissions.m_bPermissions[CGamePermission::LEADER]) {
            // "You must have the leader permission to rest the party in
            // multiplayer mode."
            strError = 17198;
            bResult = FALSE;
        } else {
            // "Are you sure you wish to rest?"
            strError = 15358;
            bResult = TRUE;
        }
    }

    // FALSE when a sprite in the array has SPLSTATE_MOLD_TOUCH or
    // SPLSTATE_SHROUD_OF_FLAME: "You may not rest at this time."
    if (!m_cObjectArray.AnySpriteIsDiseased()) {
        bResult = FALSE;
        strError = 10309;
    }

    return bResult;
}

// 0x5C3010
CStringList* CInfGame::GetPortraits()
{
    CStringList* pList = new CStringList();

    CString sFileName;
    CString sPattern;
    CString sTemp;

    sPattern = GetDirPortraits() + "*.BMP";

    INT nCount = 0;
    WIN32_FIND_DATAA findFileData;
    HANDLE hFindFile = FindFirstFileA(sPattern, &findFileData);
    if (hFindFile != INVALID_HANDLE_VALUE) {
        do {
            sFileName = findFileData.cFileName;
            if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                sFileName = sFileName.SpanExcluding(".");
                if (sFileName.GetLength() <= 8) {
                    if (nCount >= 32767) {
                        break;
                    }

                    POSITION pos = pList->GetHeadPosition();
                    while (pos != NULL) {
                        sTemp = pList->GetAt(pos);
                        if (sFileName == sTemp) {
                            break;
                        }

                        if (sFileName < pList->GetAt(pos)) {
                            pList->InsertBefore(pos, sFileName);
                            nCount++;
                            break;
                        }

                        pList->GetNext(pos);
                    }

                    if (pos == NULL) {
                        pList->AddTail(sFileName);
                        nCount++;
                    }
                }
            }
        } while (FindNextFileA(hFindFile, &findFileData));
        FindClose(hFindFile);
    }

    return pList;
}

// 0x5C3240
CStringList* CInfGame::GetImportCharacters()
{
    CStringList* pList = new CStringList();

    CString sFileName;
    CString sPattern;
    CString sTemp;

    sPattern = GetDirCharacters() + "*.chr";

    INT nCount = 0;
    WIN32_FIND_DATAA findFileData;
    HANDLE hFindFile = FindFirstFileA(sPattern, &findFileData);
    if (hFindFile != INVALID_HANDLE_VALUE) {
        do {
            sFileName = findFileData.cFileName;
            if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                sFileName = sFileName.SpanExcluding(".");
                if (sFileName.GetLength() <= 8) {
                    if (nCount >= 32767) {
                        break;
                    }

                    // NOTE: The original passes the Windows path to fopen as is;
                    // ResolvePath maps it onto the install (an identity on
                    // Windows), as the shimmed file APIs do.
                    FILE* stream = fopen(compat::ResolvePath(GetDirCharacters() + findFileData.cFileName).c_str(), "rb");
                    if (stream != NULL) {
                        unsigned char data[8];
                        size_t bytesRead = fread(data, 1, sizeof(data), stream);
                        fclose(stream);

                        if (bytesRead == 8 && memcmp(data, "CHR V2.2", 8) == 0) {
                            sFileName.MakeUpper();

                            POSITION pos = pList->GetHeadPosition();
                            while (pos != NULL) {
                                sTemp = pList->GetAt(pos);
                                if (sFileName == sTemp) {
                                    break;
                                }

                                if (sFileName < pList->GetAt(pos)) {
                                    pList->InsertBefore(pos, sFileName);
                                    nCount++;
                                    break;
                                }

                                pList->GetNext(pos);
                            }

                            if (pos == NULL) {
                                pList->AddTail(sFileName);
                                nCount++;
                            }
                        }
                    }
                }
            }
        } while (FindNextFileA(hFindFile, &findFileData));
        FindClose(hFindFile);
    }

    return pList;
}

// 0x5C3540
CStringList* CInfGame::GetSounds()
{
    CStringList* pList = new CStringList();

    CString sSoundSetDirName;
    CString sPattern;
    CString sTemp;

    sPattern = GetDirSounds() + "*";
    INT nCount = 0;

    WIN32_FIND_DATAA findFileData;
    HANDLE hFindFile = FindFirstFileA(sPattern, &findFileData);
    if (hFindFile != INVALID_HANDLE_VALUE) {
        do {
            sSoundSetDirName = findFileData.cFileName;
            if (sSoundSetDirName != "."
                && sSoundSetDirName != ".."
                && (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                sSoundSetDirName.MakeUpper();

                INT nLength = sSoundSetDirName.GetLength();
                if (nLength > 0 && nLength < 32) {
                    if (nCount >= 32766) {
                        break;
                    }

                    POSITION pos = pList->GetHeadPosition();
                    while (pos != NULL) {
                        sTemp = pList->GetAt(pos);
                        if (sSoundSetDirName == sTemp) {
                            break;
                        }

                        if (sSoundSetDirName < pList->GetAt(pos)) {
                            pList->InsertBefore(pos, sSoundSetDirName);
                            nCount++;
                            break;
                        }

                        pList->GetNext(pos);
                    }

                    if (pos == NULL) {
                        pList->AddTail(sSoundSetDirName);
                        nCount++;
                    }
                }
            }
        } while (FindNextFileA(hFindFile, &findFileData));
        FindClose(hFindFile);
    }

    return pList;
}

// 0x5C3770
STRREF CInfGame::sub_5C3770(CString a1)
{
    return 9885;
}

// 0x5C3790
void CInfGame::ChangeBiography(BYTE nFixedPartyId, CString szBiography)
{
    CBaldurMessage* pMessage = g_pBaldurChitin->GetBaldurMessage();
    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
        if (g_pChitin->cNetwork.GetSessionHosting() == FALSE) {
            if (pMessage->m_bInOnAnnounceBiography == FALSE) {
                pMessage->SendBiographyToServer(nFixedPartyId, szBiography);
                return;
            }
        } else {
            pMessage->AnnounceBiography(nFixedPartyId, szBiography);
        }
    }

    // The strings are fetched and never used; the original does the same.
    CScreenCharacter* pScreen = g_pBaldurChitin->m_pEngineCharacter;
    CString sTitle = CBaldurEngine::FetchString(0x3E0A);
    CString sDone = CBaldurEngine::FetchString(0x2E57);

    LONG nId = nFixedPartyId < 6 ? m_characters[nFixedPartyId]
                                 : CGameObjectArray::INVALID_INDEX;
    if (nId == CGameObjectArray::INVALID_INDEX) {
        // No party slot; while creating a character, edit that sprite instead.
        if (g_pBaldurChitin->GetActiveEngine() != g_pBaldurChitin->m_pEngineCreateChar) {
            return;
        }
        nId = g_pBaldurChitin->m_pEngineCreateChar->m_nGameSprite;
    }

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = m_cObjectArray.GetShare(nId, CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite), INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        STR_RES strRes;
        CTlkFileOverride& overrideFile = g_pBaldurChitin->GetTlkTable().m_override;
        if (overrideFile.Fetch(pSprite->m_baseStats.m_biography, strRes)) {
            overrideFile.Remove(pSprite->m_baseStats.m_biography);
        }

        pSprite->m_baseStats.m_biography = 0xF240 + nFixedPartyId;
        strRes.szText = szBiography;
        overrideFile.AddUserEntry(pSprite->m_baseStats.m_biography, strRes);

        m_cObjectArray.ReleaseShare(nId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    }

    // Refresh the biography edit popup when it is on top (panel id 12).
    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && g_pBaldurChitin->GetActiveEngine() == pScreen) {
        CUIPanel* pPanel = pScreen->GetTopPopup();
        if (pPanel != NULL && pPanel->m_nID == 12) {
            pScreen->GetManager()->GetPanel(12)->GetControl(0);
        }
    }
}

// The traveller moved into the destination area by the host's party
// transition (0x5C4AD0): out of its old area (marshalled to the temp folder
// once), in at the entry point (in search squares) plus its formation
// offset, at the nearest open square; the dead lie in the back, the others
// face the entry and bring the view and the selection along. The party loop
// (0x5C5233-0x5C5762) and the familiar loop (0x5C5811-0x5C5D5D) inline the
// same steps; a familiar always takes the first offset.
static void PartyTransitionMoveSprite(CGameSprite* pSprite,
    LONG nId,
    CGameArea* pArea,
    const CPoint& ptEntry,
    SHORT nFacing,
    const CPoint& ptOffset,
    BOOLEAN& bMarshalled)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    pSprite->m_triggerId = CGameObjectArray::INVALID_INDEX;
    pSprite->ClearActions(FALSE);
    pSprite->m_targetPoint.x = -1;
    pSprite->m_targetPoint.y = -1;

    CGameArea* pOldArea = pSprite->m_pArea;

    // The area left behind is saved to the temp folder once.
    CAreaFile cAreaFile;
    if (pOldArea != NULL
        && (!g_pChitin->cNetwork.m_bConnectionEstablished || g_pChitin->cNetwork.m_bIsHost == TRUE)
        && !bMarshalled
        && (pOldArea->m_header.m_flags & 0x1) == 0
        && !pOldArea->field_436) {
        BYTE* pData = NULL;
        DWORD nSize = 0;
        pOldArea->Marshal(&pData, &nSize, TRUE);
        if (pData != NULL) {
            cAreaFile.SetResRef(pOldArea->m_resRef, TRUE, TRUE);

            // NOTE: The original writes through the resource without a test;
            // it is NULL when the area file is missing. Bounded (P4-10), as
            // LeaveAreaName does.
            if (cAreaFile.pRes != NULL) {
                static_cast<CRes*>(cAreaFile.pRes)->Write(pGame->m_sTempDir, pData, nSize);
            }

            pOldArea->field_436 = 1;
            pOldArea->field_438 = pGame->m_worldTime.m_gameTime;

            delete pData;

            bMarshalled = TRUE;
        }
    }

    pGame->m_bInAreaTransition = TRUE;

    if (pSprite->m_pArea != NULL) {
        pSprite->RemoveFromArea();

        CPoint ptSquare(ptOffset.x / CAIGroup::OFFSET_MULTIPLIER + ptEntry.x,
            ptOffset.y / CAIGroup::OFFSET_MULTIPLIER + ptEntry.y);

        BYTE nPersonalSpace = pSprite->m_animation.GetPersonalSpace();
        CPoint ptOpen = pArea->m_search.GetNearestOpenSquare(ptSquare,
            pSprite->GetTerrainTable(),
            nPersonalSpace,
            -1);

        CPoint pos(ptOpen.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
            ptOpen.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2);

        if (!pSprite->Animate() && pSprite->m_animation.CanLieDown()) {
            // The dead lie in the back.
            pSprite->AddToArea(pArea, pos, 0, CGameObject::LIST_BACK);
        } else {
            pSprite->AddToArea(pArea, pos, 0, CGameObject::LIST_FRONT);
            pSprite->SetFacing(nFacing);

            if (pGame->m_gameAreas[pGame->m_visibleArea] != pArea) {
                CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
                CRect rView = pWorld->GetNewViewSize();
                if (!EqualRect(&rView, &(pArea->m_cInfinity.rViewPort))) {
                    pWorld->SetNewViewSize(rView, FALSE);
                }

                pArea->m_cInfinity.SetViewPosition(pos.x - rView.Width() / 2,
                    pos.y - rView.Height() / 2,
                    TRUE);

                if (pOldArea != pArea) {
                    pGame->UnselectAll();
                    pGame->SelectCharacter(nId, FALSE);
                }
            } else if (pOldArea == pArea) {
                const CRect& rView = pArea->m_cInfinity.rViewPort;
                pArea->m_cInfinity.SetViewPosition(pos.x - rView.Width() / 2,
                    pos.y - rView.Height() / 2,
                    TRUE);
            } else {
                pGame->SelectCharacter(nId, FALSE);
            }
        }
    }

    pGame->m_bInAreaTransition = FALSE;

    const CPoint& ptPos = pSprite->GetPos();
    if (ptPos.x != -1 || ptPos.y != -1 || pSprite->m_pArea != NULL) {
        CMessage* message = new CMessageSpriteUpdate(pSprite, nId, nId);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }
}

// 0x5C3B20
void CInfGame::sub_5C3B20(LONG nCharacterId, LONG nX, LONG nY, SHORT nDirection, CString sAreaName, CString sParchment)
{
    // TODO: Incomplete. The 4,001-byte LeaveAreaLUA transition (the
    // destination area load, the party move and the LEAVEAREALUA 88/65
    // broadcast) is a follow-up; the host permit handler (CBaldurMessage
    // 0x437950) only answers 1 and calls this.
}

// The host's transition of the whole party through a party required travel
// region `nTriggerId` (CBaldurMessage 0x438710 for the character
// `nCharacterId`, LeaveAreaName's leader): the world screen comes back, the
// auto-save, the progress screen, the destination area (the region's own
// area name) loaded and the party, then the familiars, moved to the
// region's entry point in the 2 by 3 formation; members alive in another
// area are sent after them with LeaveAreaLUA. `sAreaName` is the name
// LeaveAreaName read from the region (for field_1B86 and the clients).
//
// A single-player game is a host session (the null service provider):
// everything here runs except what needs a remote player, which is a NOTE.
//
// 0x5C4AD0
void CInfGame::sub_5C4AD0(LONG nCharacterId, LONG nTriggerId, CString sAreaName)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CBaldurMessage* pMessage = &(g_pBaldurChitin->m_cBaldurMessage);
    CGameObjectArray* pObjectArray = &(pGame->m_cObjectArray);
    BYTE rc;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 20366
    UTIL_ASSERT(pGame != NULL && pMessage != NULL);

    if (pGame->GetVisibleArea() != NULL) {
        pGame->GetVisibleArea()->sub_47A190();
    }

    if (!g_pChitin->cNetwork.m_bConnectionEstablished) {
        return;
    }

    if (g_pChitin->pActiveEngine == g_pBaldurChitin->m_pEngineMap) {
        g_pChitin->pActiveEngine->SelectEngine(g_pBaldurChitin->m_pEngineWorld);
    }

    if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE
        && g_pChitin->cNetwork.m_bIsHost == TRUE
        && g_pChitin->pActiveEngine != g_pBaldurChitin->m_pEngineWorld) {
        g_pChitin->pActiveEngine->SelectEngine(g_pBaldurChitin->m_pEngineWorld);
    }

    g_pChitin->cSoundMixer.m_nActiveArea = static_cast<DWORD_PTR>(-1);
    g_pChitin->cSoundMixer.UpdateSoundList();

    g_pBaldurChitin->m_pEngineWorld->m_weather.OnAreaChange(TRUE);

    if (g_pChitin->cNetwork.m_bIsHost == TRUE) {
        // The auto-save.
        pGame->m_sSaveGame = CInfGame::AUTO_SAVE_NAME;
        CScreenCharacter::SAVE_NAME = CInfGame::AUTO_SAVE_NAME;

        if (g_pChitin->cNetwork.m_nServiceProvider == CNetwork::SERV_PROV_NULL) {
            pGame->field_366E = 1;
            pGame->field_50DC = 0;
            if (!g_pChitin->cVideo.m_bIs3dAccelerated) {
                if (!pGame->field_50D8) {
                    pGame->SynchronousUpdate();
                }
                pGame->field_50D8 = TRUE;
            }
        }

        pGame->SaveGame(TRUE, FALSE, TRUE);

        if (g_pChitin->cNetwork.m_nServiceProvider == CNetwork::SERV_PROV_NULL
            && g_pChitin->cVideo.m_bIs3dAccelerated) {
            pGame->field_366E = 1;
            pGame->field_50DC = 1;
            // NOTE: The original tests the 3D flag again here, set; the
            // screen grab below never runs (kept).
            if (!g_pChitin->cVideo.m_bIs3dAccelerated) {
                if (!pGame->field_50D8) {
                    pGame->SynchronousUpdate();
                }
                pGame->field_50D8 = TRUE;
            }
        }

        // NOTE: CBaldurMessage 0x438F90 (with the character, the region and
        // the area name) tells the other players of the transition. Not
        // ported: there is no other player (docs/adr/0002-single-player-only.md).
    }

    BOOLEAN bMarshalled = FALSE;
    DWORD nProgressTarget = g_pChitin->cNetwork.m_bIsHost == TRUE ? 11000000 : 6000000;

    sub_59FA00(TRUE);

    g_pBaldurChitin->GetObjectGame()->m_nTravelScreenImageToUse = static_cast<BYTE>(rand() % 5);

    field_1B86 = sAreaName;

    g_pChitin->SetProgressBar(TRUE, 9891, 0, 0, TRUE, 9885, FALSE, 0, FALSE, FALSE, 255);
    g_pChitin->cProgressBar.m_nActionProgress = 0;
    g_pChitin->cProgressBar.m_nActionTarget = nProgressTarget;
    g_pChitin->cProgressBar.m_bDisableMinibars = FALSE;

    BOOLEAN bListenToJoin = FALSE;
    if (g_pChitin->cNetwork.m_bIsHost == TRUE) {
        bListenToJoin = pGame->GetMultiplayerSettings()->GetListenToJoinOption();
        pGame->GetMultiplayerSettings()->SetListenToJoinOption(FALSE, FALSE);
    }

    CGameTrigger* pTrigger;
    rc = pObjectArray->GetShare(nTriggerId,
        CGameObjectArray::THREAD_ASYNCH,
        reinterpret_cast<CGameObject**>(&pTrigger),
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        // NOTE: The original leaves the progress screen up (kept).
        return;
    }

    // NOTE: The original reads both names to their NUL; bounded to their
    // fields (P4-10).
    CString sArea(reinterpret_cast<const char*>(pTrigger->m_newArea),
        static_cast<int>(strnlen(reinterpret_cast<const char*>(pTrigger->m_newArea), RESREF_SIZE)));
    CString sEntryPoint(reinterpret_cast<const char*>(pTrigger->m_newEntryPoint),
        static_cast<int>(strnlen(reinterpret_cast<const char*>(pTrigger->m_newEntryPoint), SCRIPTNAME_SIZE)));

    pObjectArray->ReleaseShare(nTriggerId, CGameObjectArray::THREAD_ASYNCH, INFINITE);

    if (g_pChitin->cNetwork.m_bIsHost == TRUE) {
        // The host waits up to 40 rounds of 60 ms for the clients' resource
        // demands (one per other player). Alone, it does not wait.
        SHORT nPlayers = 0;
        for (INT nPlayer = 0; nPlayer < 6; nPlayer++) {
            if (g_pChitin->cNetwork.GetPlayerID(nPlayer) != 0) {
                nPlayers++;
            }
        }

        if (g_pChitin->cNetwork.m_nServiceProvider != CNetwork::SERV_PROV_NULL) {
            g_pChitin->cProgressBar.m_nWaitingReason = 19715;
            g_pChitin->cProgressBar.m_bWaiting = TRUE;
        }

        SHORT nRounds = 40;
        SHORT nDemands = 0;
        do {
            if (nDemands >= nPlayers - 1) {
                break;
            }

            INT nMsgFrom;
            DWORD dwSize;
            BYTE* pData = pMessage->PollSpecificMessageType(CBaldurMessage::MSG_TYPE_RESOURCE,
                CBaldurMessage::MSG_SUBTYPE_RESOURCE_DEMAND,
                nMsgFrom,
                dwSize);
            if (pData != NULL) {
                pMessage->OnResourceDemanded(nMsgFrom, pData, dwSize);
                delete pData;
                nDemands++;
            }

            g_pChitin->m_bDisplayStale = TRUE;
            SleepEx(60, FALSE);
            nRounds--;

            nPlayers = 0;
            for (INT nPlayer = 0; nPlayer < 6; nPlayer++) {
                if (g_pChitin->cNetwork.GetPlayerID(nPlayer) != 0) {
                    nPlayers++;
                }
            }
        } while (nRounds > 0);

        if (nPlayers > 1 && nRounds > 0) {
            for (SHORT n = nRounds; n != 0; n--) {
                g_pChitin->m_bDisplayStale = TRUE;
                SleepEx(60, FALSE);
            }
        }

        g_pChitin->cProgressBar.m_bWaiting = FALSE;
        g_pBaldurChitin->m_cCachingStatus.InvalidateScreen();
    }

    CGameArea* pArea = LoadArea(sArea, 255, FALSE, TRUE);

    CGameSprite* pLeader;
    do {
        rc = pObjectArray->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pLeader),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED);

    if (rc != CGameObjectArray::SUCCESS) {
        g_pChitin->SetProgressBar(FALSE, 0, 0, 0, FALSE, 0, FALSE, 0, FALSE, FALSE, 255);
        return;
    }

    Icewind586B70::Instance()->sub_5872C0(pLeader);

    CPoint ptEntry;
    SHORT nFacing;
    if (pArea == NULL || !pArea->GetEntryPoint(sEntryPoint, ptEntry, nFacing)) {
        g_pChitin->SetProgressBar(FALSE, 0, 0, 0, FALSE, 0, FALSE, 0, FALSE, FALSE, 255);
        pObjectArray->ReleaseDeny(nCharacterId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        return;
    }

    // The entry point in search squares.
    ptEntry.x /= CPathSearch::GRID_SQUARE_SIZEX;
    ptEntry.y /= CPathSearch::GRID_SQUARE_SIZEY;

    SHORT nFormation = g_pChitin->cNetwork.m_bConnectionEstablished
        ? CAIGroup::FORMATION_2BY3
        : pGame->m_gameSave.m_curFormation;
    CPoint* pOffsets = pGame->m_group.GetOffsets(nFormation,
        static_cast<SHORT>((nFacing + 8) * 360 / 16 % 360),
        TRUE);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 20610
    UTIL_ASSERT(pOffsets != NULL);

    SHORT nOffsetIndex = 0;
    SHORT nCount = g_pChitin->cNetwork.m_nServiceProvider != CNetwork::SERV_PROV_NULL
        ? 6
        : pGame->m_nCharacters;

    CGameSprite* pSprite;
    for (SHORT nIndex = 0; nIndex < nCount; nIndex++) {
        LONG nId;
        if (g_pChitin->cNetwork.m_nServiceProvider != CNetwork::SERV_PROV_NULL) {
            nId = nIndex < 6 ? pGame->m_characters[nIndex] : CGameObjectArray::INVALID_INDEX;
            if (nId == CGameObjectArray::INVALID_INDEX) {
                continue;
            }
        } else {
            nId = nIndex < pGame->m_nCharacters
                ? pGame->m_characterPortraits[nIndex]
                : CGameObjectArray::INVALID_INDEX;
        }

        do {
            rc = pObjectArray->GetDeny(nId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED);

        if (rc != CGameObjectArray::SUCCESS) {
            pGame->SelectToolbar();
            delete[] pOffsets;
            pObjectArray->ReleaseDeny(nCharacterId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
            return;
        }

        if (g_pChitin->cNetwork.m_nServiceProvider == CNetwork::SERV_PROV_NULL
            || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID) {
            // In a session the offset is the member's slot.
            SHORT nOffset = g_pChitin->cNetwork.m_bConnectionEstablished ? nIndex : nOffsetIndex;
            if (pSprite->m_pArea != NULL) {
                nOffsetIndex++;
            }
            PartyTransitionMoveSprite(pSprite, nId, pArea, ptEntry, nFacing, pOffsets[nOffset], bMarshalled);
        }

        pObjectArray->ReleaseDeny(nId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    }

    // The familiars, at the first offset.
    POSITION pos = pGame->m_familiars.GetHeadPosition();
    while (pos != NULL) {
        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(pGame->m_familiars.GetNext(pos)));

        do {
            rc = pObjectArray->GetDeny(nId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED);

        if (rc != CGameObjectArray::SUCCESS) {
            pGame->SelectToolbar();
            delete[] pOffsets;
            pObjectArray->ReleaseDeny(nCharacterId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
            return;
        }

        if (g_pChitin->cNetwork.m_nServiceProvider == CNetwork::SERV_PROV_NULL
            || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID) {
            PartyTransitionMoveSprite(pSprite, nId, pArea, ptEntry, nFacing, pOffsets[0], bMarshalled);
        }

        pObjectArray->ReleaseDeny(nId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    }

    delete[] pOffsets;

    pGame->SelectToolbar();

    if (g_pChitin->cNetwork.m_nServiceProvider == CNetwork::SERV_PROV_NULL
        || g_pChitin->cNetwork.m_idLocalPlayer == pLeader->m_remotePlayerID) {
        const CPoint& ptPos = pLeader->GetPos();
        if (ptPos.x != -1 || ptPos.y != -1 || pLeader->m_pArea != NULL) {
            CMessage* message = new CMessageSpriteUpdate(pLeader, nCharacterId, nCharacterId);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }

    pObjectArray->ReleaseDeny(nCharacterId, CGameObjectArray::THREAD_ASYNCH, INFINITE);

    g_pChitin->cProgressBar.m_nActionProgress = g_pChitin->cProgressBar.m_nActionTarget - 1;
    g_pChitin->m_bDisplayStale = TRUE;
    sub_59FA00(TRUE);
    g_pChitin->cProgressBar.m_nActionProgress = g_pChitin->cProgressBar.m_nActionTarget;

    if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE) {
        pMessage->SendProgressBarStatus(g_pChitin->cProgressBar.m_nActionProgress,
            g_pChitin->cProgressBar.m_nActionTarget,
            g_pChitin->cProgressBar.m_bWaiting,
            g_pChitin->cProgressBar.m_nWaitingReason,
            g_pChitin->cProgressBar.m_bTimeoutVisible,
            g_pChitin->cProgressBar.m_nSecondsToTimeout);
    }

    // The entry point in pixels again.
    ptEntry.x *= CPathSearch::GRID_SQUARE_SIZEX;
    ptEntry.y *= CPathSearch::GRID_SQUARE_SIZEY;

    if (g_pChitin->cNetwork.m_bIsHost == TRUE) {
        g_pChitin->cProgressBar.m_bTimeoutVisible = TRUE;
        g_pChitin->cProgressBar.m_nSecondsToTimeout = pMessage->m_dwSignalSecondsToTimeout;

        // NOTE: The BYTE signal 51 at 0x84CFBF.
        if (!pMessage->NonBlockingWaitForSignal(CBaldurMessage::SIGNAL_SERVER, 51)) {
            // NOTE: Not ported: the host's wait for every client's signal
            // (0x5C5FEE-0x5C686A: the timeout count shown, the slow players
            // kicked, their progress, resource demands and sprite updates
            // handled, the characters of players gone taken over). It runs
            // only while another player has not signalled; with no other
            // player (docs/adr/0002-single-player-only.md) the wait answers
            // at once.
        }

        // NOTE: Not ported: the sprite updates the clients sent meanwhile
        // (0x5C686F-0x5C6950, MSG_TYPE_CMESSAGE, sprite update); only another
        // player sends them.

        // NOTE: The BYTE signal 52 at 0x84CFC0.
        pMessage->SendSignal(CBaldurMessage::SIGNAL_ALL_CLIENTS, 52);

        pGame->GetMultiplayerSettings()->SetListenToJoinOption(bListenToJoin, FALSE);
    } else if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE) {
        // NOTE: Not ported: a client's wait for the host (0x5C6986-0x5C6CAE,
        // its progress and sprite updates, then its signal). iwd2r has no
        // client (docs/adr/0002-single-player-only.md).
    }

    g_pChitin->cProgressBar.m_bTimeoutVisible = FALSE;
    g_pChitin->cProgressBar.m_nActionProgress = g_pChitin->cProgressBar.m_nActionProgress
        + (g_pChitin->cProgressBar.m_nActionTarget - g_pChitin->cProgressBar.m_nActionProgress);
    g_pChitin->m_bDisplayStale = TRUE;
    sub_59FA00(TRUE);
    g_pChitin->SetProgressBar(FALSE, 0, 0, 0, FALSE, 0, FALSE, 0, FALSE, FALSE, 255);

    g_pChitin->cSoundMixer.m_nActiveArea = reinterpret_cast<DWORD_PTR>(pArea);
    g_pChitin->cSoundMixer.UpdateSoundList();

    g_pBaldurChitin->m_pEngineWorld->m_weather.OnAreaChange(FALSE);

    if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE
        && g_pChitin->cNetwork.m_bIsHost == TRUE) {
        // The members alive in another area follow (LeaveAreaLUA to the
        // entry point).
        CInfGame* pHostGame = g_pBaldurChitin->GetObjectGame();

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 21370
        UTIL_ASSERT(pHostGame != NULL);

        // NOTE: The BYTE count at 0x84D59D (6).
        for (SHORT nSlot = 0; nSlot < 6; nSlot++) {
            LONG nId = nSlot < 6 ? pHostGame->m_characters[nSlot] : CGameObjectArray::INVALID_INDEX;
            BOOLEAN bElsewhere = FALSE;
            BOOLEAN bLocal = FALSE;

            do {
                rc = pHostGame->m_cObjectArray.GetShare(nId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc != CGameObjectArray::SUCCESS) {
                continue;
            }

            if (pSprite->Animate()
                && pSprite->m_pArea != NULL
                && pSprite->m_pArea != pArea) {
                bElsewhere = TRUE;
                bLocal = g_pChitin->cNetwork.m_nServiceProvider == CNetwork::SERV_PROV_NULL
                    || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID;
            }

            pHostGame->m_cObjectArray.ReleaseShare(nId, CGameObjectArray::THREAD_ASYNCH, INFINITE);

            if (bElsewhere != TRUE) {
                continue;
            }

            if (!bLocal) {
                pHostGame->GetMultiplayerSettings()->SetCharacterControlledByPlayer(nSlot,
                    static_cast<BYTE>(g_pChitin->cNetwork.m_nLocalPlayer),
                    TRUE,
                    TRUE);
            }

            do {
                rc = pHostGame->m_cObjectArray.GetDeny(nId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc != CGameObjectArray::SUCCESS) {
                continue;
            }

            pSprite->m_cLastSpriteUpdate.Initialize(TRUE);

            CAIAction follow;
            follow.m_actionID = CAIAction::LEAVEAREALUA;
            follow.SetString1(sArea);
            follow.m_dest = ptEntry;
            follow.m_specificID = 0;
            follow.m_specificID2 = 1;
            follow.m_specificID3 = 0;
            follow.m_internalFlags = 0;

            pSprite->ClearActions(FALSE);
            pSprite->m_targetPoint.x = -1;
            pSprite->m_targetPoint.y = -1;
            pSprite->m_interrupt = TRUE;
            pSprite->SetCurrAction(follow);
            pSprite->ExecuteAction();

            CGameArea* pVisibleArea = pHostGame->m_gameAreas[pHostGame->m_visibleArea];
            if (pVisibleArea->m_nCharacters == 0) {
                // Nobody left on view: the view goes with this member.
                pHostGame->UnselectAll();
                pVisibleArea->m_bPicked = FALSE;
                pVisibleArea->m_iPicked = CGameObjectArray::INVALID_INDEX;
                pVisibleArea->m_nToolTip = 0;
                pVisibleArea->OnDeactivation();
                pHostGame->m_visibleArea = pSprite->m_pArea->m_id;
                pHostGame->m_gameAreas[pHostGame->m_visibleArea]->OnActivation();
            }

            pHostGame->m_cObjectArray.ReleaseDeny(nId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        }
    }
}

// 0x5C72D0
BOOLEAN CInfGame::GetGameSpyCharacterInformation(INT nCharacterSlot, CString& sName, CString& sRace, CString& sClass, CString& sLevel)
{
    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();
    CString sTemp;

    LONG nCharacterId = GetFixedOrderCharacterId(nCharacterSlot);

    if (nCharacterId == CGameObjectArray::INVALID_INDEX) {
        return FALSE;
    }

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = m_cObjectArray.GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    BYTE nClass = pSprite->m_derivedStats.GetBestClass();

    sName = pSprite->GetName();
    ruleTables.GetRaceStringMixed(pSprite->m_liveTypeAI.m_nRace, sClass, 0);
    ruleTables.GetClassStringGameSpy(nClass, pSprite->m_baseStats.m_specialization, sClass);

    sTemp.Format("/%d", pSprite->m_derivedStats.m_nLevel);
    sLevel += sTemp;

    m_cObjectArray.ReleaseDeny(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return TRUE;
}

// 0x5C7440
//
// An item that could not be placed where the loot/give code intended must not
// leak.  Droppable items go on the disposable list; an undroppable (flag 0x1)
// item is force-fit into a free personal slot on the looter -- bumping a
// droppable occupant if necessary -- so quest items can never be lost.
void CInfGame::PutBackItem(CItem* pItem, LONG nLooterId)
{
    if ((pItem->GetFlagsFile() & 0x1) == 0) {
        AddDisposableItem(pItem);
        return;
    }

    CGameSprite* pLooter;
    BYTE rc;
    do {
        rc = m_cObjectArray.GetShare(nLooterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pLooter),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);
    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    INT nEmptySlot = -1;
    INT nBumpSlot = -1;
    for (INT nSlot = 18; nSlot < 34; nSlot++) {
        CItem* pSlotItem = pLooter->GetEquipment()->m_items[nSlot];
        if (pSlotItem == NULL) {
            nEmptySlot = nSlot;
        } else if ((pSlotItem->GetFlagsFile() & 0x1) == 0) {
            nBumpSlot = nSlot;
        }
    }

    if (nEmptySlot < 0) {
        if (nBumpSlot < 0) {
            AddDisposableItem(pItem);
            m_cObjectArray.ReleaseShare(nLooterId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
            return;
        }
        CItem* pBumped = pLooter->GetEquipment()->m_items[nBumpSlot];
        if (pBumped != NULL) {
            AddDisposableItem(pBumped);
        }
        pLooter->GetEquipment()->m_items[nBumpSlot] = pItem;
    } else {
        pLooter->GetEquipment()->m_items[nEmptySlot] = pItem;
    }

    CMessage* message = new CMessageSpriteEquipment(pLooter, nLooterId, nLooterId);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    m_cObjectArray.ReleaseShare(nLooterId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
}

// 0x5C76B0
void CInfGame::MultiplayerSetCharacterCreationLocation()
{
    CResRef areaResRef;
    CString sAreaString;
    BOOLEAN bValueSet = FALSE;

    for (BYTE nIndex = 0; nIndex < 6; nIndex++) {
        if (bValueSet) {
            break;
        }

        // NOTE: Uninline `GetFixedOrderCharacterId`.
        LONG nCharacterId = g_pBaldurChitin->GetObjectGame()->GetFixedOrderCharacterId(nIndex);

        if (nCharacterId != CGameObjectArray::INVALID_INDEX) {
            CGameSprite* pSprite;

            BYTE rc;
            do {
                rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                CGameArea* pArea = pSprite->GetArea();
                if (pArea != NULL) {
                    areaResRef = pArea->m_resRef;
                    areaResRef.CopyToString(sAreaString);

                    CPoint ptStart = pSprite->GetPos();
                    g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->SetCharacterCreationLocation(sAreaString,
                        ptStart);

                    bValueSet = TRUE;

                    // TODO: Probably wrong, object released only when area is
                    // not null.
                    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                }
            }
        }
    }
}

// 0x5C7830
BOOL CInfGame::IsItemExclusive(CGameSprite* pSprite, INT slotNum, CItem* pItem, STRREF& strError)
{
    const CRuleTables& cRule = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    strError = -1;

    INT slots[] = {
        7,
        8,
        1,
        4,
        0,
    };

    if (pItem == NULL || !cRule.IsItemExclusive(pItem)) {
        return FALSE;
    }

    for (int index = 0; index < 5; index++) {
        if (slotNum != slots[index]
            && pSprite->GetEquipment()->m_items[slots[index]] != NULL
            && cRule.IsItemExclusive(pSprite->GetEquipment()->m_items[slots[index]])) {
            strError = 20685;
            return TRUE;
        }
    }

    return FALSE;
}

// 0x5C78F0
CWorldMap* CInfGame::GetWorldMap(CString sArea)
{
    CString v1;

    // NOTE: Not sure what first condition is for.
    if (sArea
        && sArea[0] != '\0'
        && sArea[1] != '\0'
        && sArea[2] != '\0') {
        v1 = sArea.Mid(2);
        if (atoi(v1) >= 9000) {
            return &m_cOtherWorldMap;
        }
    }

    return &m_cWorldMap;
}

// 0x5C79C0
BOOL CInfGame::sub_5C79C0(CString sArea)
{
    CString v1;

    // NOTE: Not sure what first condition is for.
    if (sArea
        && sArea[0] != '\0'
        && sArea[1] != '\0'
        && sArea[2] != '\0') {
        v1 = sArea.Mid(2);
        if (atoi(v1) >= 9000) {
            return TRUE;
        }
    }

    return FALSE;
}

// 0x5C7B10
INT CInfGame::AddItemToBag(const CResRef& cResBag, CItem* pItem, STRREF& strError)
{
    INT nCount = 0;
    INT nOriginalUsage = 0;

    strError = -1;

    if (g_pChitin->cNetwork.m_bConnectionEstablished == FALSE) {
        CStore cStore(cResBag);
        if (cStore.GetType() == 4 && cStore.IsItemAcceptable(pItem)) {
            // NOTE: The bag's capacity is the WORD at the header's +0x94; the
            // original compares it with the number of items in the bag.
            WORD nCapacity = static_cast<WORD>(cStore.m_header.field_94);
            if (nCapacity != 0 && nCapacity != cStore.GetNumItems()) {
                nCount = 1;
                if (pItem->GetMaxStackable() > 1) {
                    nCount = pItem->GetUsageCount(0);
                }

                if (cStore.GetNumItems() + nCount > nCapacity) {
                    nCount = nCapacity - cStore.GetNumItems();
                }

                pItem->SetUsageCount(0, static_cast<WORD>(nCount));
                cStore.AddItemExt(*pItem, 1);
            } else {
                // "Container Is Full"
                strError = 24889;
            }
        } else {
            // "Wrong Item Type"
            strError = 26285;
        }

        if (strError == -1) {
            cStore.Marshal(m_sTempDir);
        }

        return nCount;
    }

    // A session: a single-player game runs this as the host. The host fills
    // the server's copy of the bag; a client asks the server for it first
    // and works on its local copy.
    BOOL bStoreDemanded = FALSE;
    CStore cStore;
    if (g_pChitin->cNetwork.GetSessionHosting()) {
        DemandServerStore(cResBag, TRUE);
        cStore.SetResRef(cResBag);
    } else {
        cStore.SetResRef(cResBag);
        if (!cStore.m_bLocalCopy || memcmp(cStore.m_pVersion, "STORV9.0", 8) != 0) {
            if (!g_pBaldurChitin->GetBaldurMessage()->DemandResourceFromServer(cResBag.GetResRefStr(),
                    1014,
                    TRUE,
                    TRUE,
                    TRUE)) {
                g_pChitin->cNetwork.CloseSession(TRUE);
                return 0;
            }

            cStore.SetResRef(cResBag);
            bStoreDemanded = TRUE;
        }
    }

    if (cStore.GetType() == 4 && cStore.IsItemAcceptable(pItem)) {
        // NOTE: The bag's capacity is the WORD at the header's +0x94; the
        // original compares it with the number of items in the bag.
        WORD nCapacity = static_cast<WORD>(cStore.m_header.field_94);
        if (nCapacity == 0 || nCapacity == cStore.GetNumItems()) {
            // "Container Is Full"
            strError = 24889;
        } else {
            if (pItem->GetMaxStackable() < 2) {
                nCount = 1;
            } else {
                nCount = pItem->GetUsageCount(0);
                nOriginalUsage = nCount;

                // NOTE: The capacity of a bag counts stacks, so a stackable
                // item's count is divided by its maximum stack first.
                if (nCount / pItem->GetMaxStackable() + cStore.GetNumItems() > nCapacity) {
                    nCount = nCapacity - cStore.GetNumItems();
                }

                pItem->SetUsageCount(0, static_cast<WORD>(nCount));
            }

            if (g_pChitin->cNetwork.GetSessionHosting() == FALSE && bStoreDemanded == FALSE) {
                bStoreDemanded = TRUE;
                CMessageStoreDemand* pDemandMessage = new CMessageStoreDemand(cResBag, -1, -1);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(pDemandMessage, FALSE);
            }

            // NOTE: `CMessageStoreAddItem` is BROADCAST_FORCED_OTHERS, so
            // the message handler deletes it in a single-player session.
            CMessageStoreAddItem* pAddMessage = new CMessageStoreAddItem(cResBag,
                pItem->GetItemFile(),
                -1,
                -1);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pAddMessage, FALSE);

            if (g_pChitin->cNetwork.GetSessionHosting()) {
                CStore* pServerStore = GetServerStore(cResBag);

                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
                // __LINE__: 22140
                UTIL_ASSERT(pServerStore != NULL);

                pServerStore->AddItemExt(*pItem, 1);
            } else {
                CStore::InvalidateStore(cResBag);
            }

            if (pItem->GetMaxStackable() > 1) {
                pItem->SetUsageCount(0, static_cast<WORD>(nOriginalUsage));
            }
        }
    } else {
        // "Wrong Item Type"
        strError = 26285;
    }

    if (g_pChitin->cNetwork.GetSessionHosting()) {
        ReleaseServerStore(cResBag);
    } else if (bStoreDemanded) {
        CMessageStoreRelease* pReleaseMessage = new CMessageStoreRelease(cResBag, -1, -1);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pReleaseMessage, FALSE);
    }

    return nCount;
}

// 0x5C8190
DWORD CInfGame::FindItemInStore(const CResRef& cResStore, const CResRef& cResItem, BOOL checkForIdentified)
{
    if (g_pChitin->cNetwork.GetSessionOpen()) {
        BOOL bStoreDemanded = FALSE;
        CStore cStore;
        if (g_pChitin->cNetwork.GetSessionHosting()) {
            DemandServerStore(cResStore, TRUE);
            cStore.SetResRef(cResStore);
        } else {
            cStore.SetResRef(cResStore);
            if (!cStore.m_bLocalCopy || memcmp(cStore.m_pVersion, "STORV9.0", 8) != 0) {
                if (!g_pBaldurChitin->GetBaldurMessage()->DemandResourceFromServer(cResStore.GetResRefStr(), 1014, TRUE, TRUE, TRUE)) {
                    g_pChitin->cNetwork.CloseSession(TRUE);
                    return 0;
                }
                cStore.SetResRef(cResStore);
                bStoreDemanded = TRUE;
            }
        }

        CResRef curItemRef;
        DWORD inStock = 0;
        CItem tempItem;

        for (INT nIndex = 0; nIndex < cStore.GetNumItems(); nIndex++) {
            curItemRef = cStore.GetItemId(nIndex);
            if (curItemRef == cResItem) {
                if (checkForIdentified) {
                    cStore.GetItem(nIndex, tempItem);
                    if ((tempItem.m_flags & 0x1) != 0) {
                        inStock += cStore.GetItemNumInStock(nIndex);
                    }
                } else {
                    inStock += cStore.GetItemNumInStock(nIndex);
                }
            }
        }

        if (g_pChitin->cNetwork.GetSessionHosting()) {
            ReleaseServerStore(cResStore);
        } else {
            if (bStoreDemanded) {
                CMessage* message = new CMessageStoreRelease(cResStore, -1, -1);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }

        return inStock;
    } else {
        CStore cStore(cResStore);
        CResRef curItemRef;
        DWORD inStock = 0;

        for (INT nIndex = 0; nIndex < cStore.GetNumItems(); nIndex++) {
            curItemRef = cStore.GetItemId(nIndex);
            if (curItemRef == cResItem) {
                inStock += cStore.GetItemNumInStock(nIndex);
            }
        }

        return inStock;
    }
}

// 0x5C8510
SHORT CInfGame::TakeItemFromStore(const CResRef& cResStore, const CResRef& cResItem, LONG number)
{
    CItem tempItem;
    CMessage* message;

    if (g_pChitin->cNetwork.GetSessionOpen()) {
        BOOL bStoreDemanded = FALSE;
        CStore cStore;

        if (g_pChitin->cNetwork.GetSessionHosting()) {
            DemandServerStore(cResStore, TRUE);
            cStore.SetResRef(cResStore);
        } else {
            cStore.SetResRef(cResStore);
            if (!cStore.m_bLocalCopy || memcmp(cStore.m_pVersion, "STORV9.0", 8) != 0) {
                if (!g_pBaldurChitin->GetBaldurMessage()->DemandResourceFromServer(cResStore.GetResRefStr(), 1014, TRUE, TRUE, TRUE)) {
                    g_pChitin->cNetwork.CloseSession(TRUE);
                    return 0;
                }

                cStore.SetResRef(cResStore);
                bStoreDemanded = TRUE;
            }
        }

        CResRef curResItem;
        CStore* pStore;
        for (INT nIndex = 0; nIndex < cStore.GetNumItems(); nIndex++) {
            curResItem = cStore.GetItemId(nIndex);
            if (curResItem == cResItem) {
                DWORD inStock = cStore.GetItemNumInStock(nIndex);
                cStore.GetItem(nIndex, tempItem);

                if (!g_pChitin->cNetwork.GetSessionHosting() && !bStoreDemanded) {
                    bStoreDemanded = TRUE;
                    message = new CMessageStoreDemand(cResStore, -1, -1);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                }

                if (g_pChitin->cNetwork.GetSessionHosting()) {
                    pStore = g_pBaldurChitin->GetObjectGame()->GetServerStore(cResStore);

                    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
                    // __LINE__: 22412
                    UTIL_ASSERT(pStore != NULL);
                }

                // FIXME: Looks wrong, `removed` is reset on every iteration,
                // but probably should be accumulated.
                DWORD removed = 0;
                // NOTE: Unsigned compare.
                while (removed < inStock && removed < static_cast<DWORD>(number)) {
                    message = new CMessageStoreRemoveItem(cResStore,
                        cResItem,
                        tempItem.m_flags,
                        -1,
                        1,
                        -1,
                        -1);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

                    if (g_pChitin->cNetwork.GetSessionHosting()) {
                        pStore->RemoveItemExt(cResItem, tempItem.m_flags, -1, 1, NULL);
                    }
                }

                if (!g_pChitin->cNetwork.GetSessionHosting()) {
                    CStore::InvalidateStore(cResStore);
                }

                // NOTE: Unsigned compare.
                if (removed >= static_cast<DWORD>(number)) {
                    if (g_pChitin->cNetwork.GetSessionHosting()) {
                        g_pBaldurChitin->GetObjectGame()->ReleaseServerStore(cResStore);
                    } else {
                        if (bStoreDemanded) {
                            message = new CMessageStoreRelease(cResStore, -1, -1);
                            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                        }
                    }

                    return static_cast<SHORT>(removed);
                }
            }
        }

        if (g_pChitin->cNetwork.GetSessionHosting()) {
            g_pBaldurChitin->GetObjectGame()->ReleaseServerStore(cResStore);
        } else {
            if (bStoreDemanded) {
                message = new CMessageStoreRelease(cResStore, -1, -1);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }
        return 0;
    } else {
        CStore cStore(cResStore);
        CResRef curItemRef;
        for (INT nIndex = 0; nIndex < cStore.GetNumItems(); nIndex++) {
            curItemRef = cStore.GetItemId(nIndex);
            cStore.GetItem(nIndex, tempItem);
            if (curItemRef == cResItem) {
                DWORD inStock = cStore.GetItemNumInStock(nIndex);
                // FIXME: Looks wrong, `removed` is reset on every iteration,
                // but probably should be accumulated.
                DWORD removed = 0;
                // NOTE: Unsigned compare.
                while (removed < static_cast<DWORD>(number) && removed < inStock) {
                    cStore.RemoveItemExt(cResItem, tempItem.m_flags, -1, 1, NULL);
                    removed++;
                }

                // NOTE: Unsigned compare.
                if (removed >= static_cast<DWORD>(number)) {
                    cStore.Marshal(m_sTempDir);
                    return static_cast<SHORT>(removed);
                }
            }
        }
        cStore.Marshal(m_sTempDir);
        return 0;
    }
}

// 0x5C93E0
INT CInfGame::sub_5C93E0()
{
    INT nCount = 0;

    for (SHORT nPortrait = 0; nPortrait < m_nCharacters; nPortrait++) {
        LONG nCharacterId = GetCharacterId(nPortrait);

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            if (pSprite->field_70F2 == 1) {
                nCount++;
            }

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    return nCount;
}

// NOTE: Somewhat broken version of `LoadMultiPlayerPermissions`.
//
// 0x5C9490
void CInfGame::ResetMultiPlayerPermissions()
{
    if (!g_pBaldurChitin->cNetwork.GetSessionOpen()
        || !g_pBaldurChitin->cNetwork.GetSessionHosting()) {
        return;
    }

    CMultiplayerSettings* pSettings = GetMultiplayerSettings();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 22952
    UTIL_ASSERT(pSettings != NULL);

    CString sPermissions;
    CString sDefault("");

    GetPrivateProfileStringA("Multiplayer",
        "Default Permissions",
        sDefault,
        sPermissions.GetBuffer(10),
        10,
        g_pBaldurChitin->GetIniFileName());
    sPermissions.ReleaseBuffer(-1);

    if (sPermissions.GetLength() == 8) {
        BOOL bPurchasing = sPermissions[0] == '1';
        BOOL bAreaTransition = sPermissions[1] == '1';
        BOOL bDialog = sPermissions[2] == '1';
        BOOL bCharRecords = sPermissions[3] == '1';
        BOOL bPausing = sPermissions[4] == '1';
        BOOL bGroupPool = sPermissions[5] == '1';
        BOOL bLeader = sPermissions[6] == '1';
        BOOL bModifyChars = sPermissions[7] == '1';

        // FIXME: Individual permission values are used as player slots. So
        // depending on the value appropriate permission is set either on
        // player 0 or 1.

        pSettings->SetPermission(bPurchasing,
            CGamePermission::PURCHASING,
            bPurchasing,
            TRUE);
        pSettings->SetPermission(bAreaTransition,
            CGamePermission::AREA_TRANSITION,
            bAreaTransition,
            TRUE);
        pSettings->SetPermission(bDialog,
            CGamePermission::DIALOG,
            bDialog,
            TRUE);
        pSettings->SetPermission(bCharRecords,
            CGamePermission::CHAR_RECORDS,
            bCharRecords,
            TRUE);
        pSettings->SetPermission(bPausing,
            CGamePermission::PAUSING,
            bPausing,
            TRUE);
        pSettings->SetPermission(bGroupPool,
            CGamePermission::GROUP_POOL,
            bGroupPool,
            TRUE);
        pSettings->SetPermission(bLeader,
            CGamePermission::LEADER,
            bLeader,
            TRUE);
        pSettings->SetPermission(bModifyChars,
            CGamePermission::MODIFY_CHARS,
            bModifyChars,
            TRUE);
    }

    INT nRestrictStore = GetPrivateProfileIntA("Multiplayer",
        "Restrict Stores",
        0,
        g_pBaldurChitin->GetIniFileName());
    pSettings->SetRestrictStoreOption(nRestrictStore != 0);

    BYTE nImportingBitField = static_cast<BYTE>(GetPrivateProfileIntA("Multiplayer",
        "Import Character",
        CMultiplayerSettings::IMPORT_ALL,
        g_pBaldurChitin->GetIniFileName()));
    pSettings->SetImportingCharacterOption(nImportingBitField);
}

// 0x5C9720
INT CInfGame::GetNumberOfItemsInBag(const CResRef& storeResRef, CString a2)
{
    CMessage* message;
    INT nNumberOfItemsInBag = 0;

    if (!g_pChitin->cNetwork.GetSessionOpen()) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 23072
        UTIL_ASSERT_MSG(FALSE, "Very bad things are about to happen in GetNumberOfItemsInBag");
    }

    BOOL demanded = FALSE;
    CStore store;
    if (g_pChitin->cNetwork.GetSessionHosting()) {
        DemandServerStore(storeResRef, TRUE);
        store.SetResRef(storeResRef);
    } else {
        store.SetResRef(storeResRef);
        if (!store.m_bLocalCopy || memcmp(store.m_pVersion, "STORV9.0", 8) != 0) {
            if (!g_pBaldurChitin->GetBaldurMessage()->DemandResourceFromServer(storeResRef.GetResRefStr(),
                    1014,
                    TRUE,
                    TRUE,
                    TRUE)) {
                g_pChitin->cNetwork.CloseSession(TRUE);
                return FALSE;
            }

            store.SetResRef(storeResRef);
            demanded = TRUE;
        }
    }

    CResRef itemResRef;

    if (g_pChitin->cNetwork.GetSessionHosting() || demanded) {
        store.InvalidateStore(storeResRef);
    } else {
        demanded = TRUE;
        message = new CMessageStoreDemand(storeResRef, -1, -1);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    for (INT nIndex = 0; nIndex < store.GetNumItems(); nIndex++) {
        itemResRef = store.GetItemId(nIndex);

        // FIXME: Temporary instance, should be on stack.
        CItem* pItem = new CItem();
        store.GetItem(nIndex, *pItem);
        if (pItem->GetResRef() != a2) {
            nNumberOfItemsInBag += store.GetItemNumInStock(nIndex);
        }

        // FIXME: Probably leaking `pItem`.
    }

    if (g_pChitin->cNetwork.GetSessionHosting()) {
        g_pBaldurChitin->GetObjectGame()->ReleaseServerStore(storeResRef);
    } else {
        if (demanded) {
            message = new CMessageStoreRelease(storeResRef, -1, -1);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }

    return nNumberOfItemsInBag;
}

// 0x5C9AC0
LONG CInfGame::GetAveragePartyLevel()
{
    LONG nAverageLevel = 0;

    for (SHORT nPortrait = 0; nPortrait < m_nCharacters; nPortrait++) {
        // NOTE: Uninline.
        LONG nCharacterId = GetCharacterId(nPortrait);

        if (nCharacterId != CGameObjectArray::INVALID_INDEX) {
            CGameSprite* pSprite;
            m_cObjectArray.GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);

            nAverageLevel += pSprite->GetLevel();

            m_cObjectArray.ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    if (nAverageLevel == 0 || nAverageLevel / m_nCharacters < 1) {
        return 1;
    }

    return nAverageLevel / m_nCharacters;
}

// NOTE: Unclear why `nClass` is passed as pointer.
// TODO: Move to `CRuleTables`.
//
// 0x5CA5D0
UINT CInfGame::GetSpellcasterIndex(const BYTE& nClass)
{
    switch (nClass) {
    case CAIOBJECTTYPE_C_BARD:
        return 0;
    case CAIOBJECTTYPE_C_CLERIC:
        return 1;
    case CAIOBJECTTYPE_C_DRUID:
        return 2;
    case CAIOBJECTTYPE_C_PALADIN:
        return 3;
    case CAIOBJECTTYPE_C_RANGER:
        return 4;
    case CAIOBJECTTYPE_C_SORCERER:
        return 5;
    case CAIOBJECTTYPE_C_WIZARD:
        return 6;
    }

    CString sError;
    sError.Format("Invalid spell caster index: %d", nClass);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 23583
    UTIL_ASSERT_MSG(FALSE, sError);
}

// NOTE: Unclear why `nList` is passed as pointer.
// TODO: Move to `CRuleTables`.
//
// 0x5CA730
BYTE CInfGame::GetSpellcasterClass(const UINT& nIndex)
{
    switch (nIndex) {
    case 0:
        return CAIOBJECTTYPE_C_BARD;
    case 1:
        return CAIOBJECTTYPE_C_CLERIC;
    case 2:
        return CAIOBJECTTYPE_C_DRUID;
    case 3:
        return CAIOBJECTTYPE_C_PALADIN;
    case 4:
        return CAIOBJECTTYPE_C_RANGER;
    case 5:
        return CAIOBJECTTYPE_C_SORCERER;
    case 6:
        return CAIOBJECTTYPE_C_WIZARD;
    }

    CString sError;
    sError.Format("Invalid spell caster class: %d", nIndex);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 23624
    UTIL_ASSERT_MSG(FALSE, sError);
}

// NOTE: Unclear why `nIndex` is passed as pointer.
// TODO: Move to `CRuleTables`.
//
// 0x5CA870
DWORD CInfGame::GetSpellcasterClassMask(const UINT& nIndex)
{
    switch (nIndex) {
    case 0:
        return CLASSMASK_BARD;
    case 1:
        return CLASSMASK_CLERIC;
    case 2:
        return CLASSMASK_DRUID;
    case 3:
        return CLASSMASK_PALADIN;
    case 4:
        return CLASSMASK_RANGER;
    case 5:
        return CLASSMASK_SORCERER;
    case 6:
        return CLASSMASK_WIZARD;
    }

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 23663
    UTIL_ASSERT_MSG(FALSE, "Invalid spell caster class mask");
}

// NOTE: Unclear why `a1` is passed as pointer.
// TODO: Move to `CRuleTables`.
//
// 0x5CA910
UINT CInfGame::GetSpellcasterDomainIndex(const DWORD& nSpecialization)
{
    if ((nSpecialization & SPECMASK_CLERIC_ILMATER) != 0) {
        return 0;
    } else if ((nSpecialization & SPECMASK_CLERIC_LATHANDER) != 0) {
        return 1;
    } else if ((nSpecialization & SPECMASK_CLERIC_SELUNE) != 0) {
        return 2;
    } else if ((nSpecialization & SPECMASK_CLERIC_HELM) != 0) {
        return 3;
    } else if ((nSpecialization & SPECMASK_CLERIC_OGHMA) != 0) {
        return 4;
    } else if ((nSpecialization & SPECMASK_CLERIC_TEMPUS) != 0) {
        return 5;
    } else if ((nSpecialization & SPECMASK_CLERIC_BANE) != 0) {
        return 6;
    } else if ((nSpecialization & SPECMASK_CLERIC_MASK) != 0) {
        return 7;
    } else if ((nSpecialization & SPECMASK_CLERIC_TALOS) != 0) {
        return 8;
    }

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 23714
    UTIL_ASSERT_MSG(FALSE, "Invalid spell caster domain index");
}

// 0x5CA9E0
DWORD CInfGame::GetClassMask(const BYTE& nClass)
{
    switch (nClass) {
    case CAIOBJECTTYPE_C_BARBARIAN:
        return CLASSMASK_BARBARIAN;
    case CAIOBJECTTYPE_C_BARD:
        return CLASSMASK_BARD;
    case CAIOBJECTTYPE_C_CLERIC:
        return CLASSMASK_CLERIC;
    case CAIOBJECTTYPE_C_DRUID:
        return CLASSMASK_DRUID;
    case CAIOBJECTTYPE_C_FIGHTER:
        return CLASSMASK_FIGHTER;
    case CAIOBJECTTYPE_C_MONK:
        return CLASSMASK_MONK;
    case CAIOBJECTTYPE_C_PALADIN:
        return CLASSMASK_PALADIN;
    case CAIOBJECTTYPE_C_RANGER:
        return CLASSMASK_RANGER;
    case CAIOBJECTTYPE_C_ROGUE:
        return CLASSMASK_ROGUE;
    case CAIOBJECTTYPE_C_SORCERER:
        return CLASSMASK_SORCERER;
    case CAIOBJECTTYPE_C_WIZARD:
        return CLASSMASK_WIZARD;
    }

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 23834
    UTIL_ASSERT_MSG(FALSE, "CInfGame::GetClassMask() - Invalid class mask!");
}

// 0x5CAAD0
BYTE CInfGame::GetSpellType(const CResRef& resRef)
{
    UINT nID;

    // NOTE: Uninline.
    if (m_songs.Find(resRef, nID)) {
        return 3;
    }

    // NOTE: Uninline.
    if (m_shapeshifts.Find(resRef, nID)) {
        return 4;
    }

    if (m_innateSpells.Find(resRef, nID)) {
        return 2;
    }

    if (m_spells.Find(resRef, nID)) {
        return 1;
    }

    CString sError(" ");
    sError.Format("*** Invalid spell: %s\n", (LPCSTR)resRef.GetResRefStr());

    // TODO: Report error.

    return 0;
}

// 0x5CAC30
INT CInfGame::GetSpellLevel(const CResRef& resRef, BYTE nClass, DWORD nSpecialization)
{
    UINT nID;

    // NOTE: Uninline.
    if (!m_spells.Find(resRef, nID)) {
        CString sError;
        sError.Format("Spell not in master list - Invalid spell: %s", (LPCSTR)resRef.GetResRefStr());

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 23940
        UTIL_ASSERT_MSG(FALSE, (LPCSTR)sError);
    }

    if (nSpecialization != 0) {
        UINT nDomainIndex = GetSpellcasterDomainIndex(nSpecialization);

        for (UINT nLevel = 0; nLevel < m_spellsByDomain[nDomainIndex].m_nHighestLevel; nLevel++) {
            CSpellList* pSpells = &(m_spellsByDomain[nDomainIndex].m_lists[nLevel]);
            for (UINT nIndex = 0; nIndex < pSpells->m_nCount; nIndex++) {
                if (pSpells->m_pList[nIndex].m_nID == nID) {
                    return nLevel;
                }
            }
        }
    } else {
        UINT nClassIndex = GetSpellcasterIndex(nClass);

        for (UINT nLevel = 0; nLevel < m_spellsByClass[nClassIndex].m_nHighestLevel; nLevel++) {
            CSpellList* pSpells = &(m_spellsByClass[nClassIndex].m_lists[nLevel]);
            for (UINT nIndex = 0; nIndex < pSpells->m_nCount; nIndex++) {
                if (pSpells->m_pList[nIndex].m_nID == nID) {
                    return nLevel;
                }
            }
        }
    }

    return CSPELLLIST_MAX_LEVELS;
}

// 0x5CADF0
void CInfGame::sub_5CADF0()
{
    // FIXME: What for?
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    for (SHORT nPortrait = 0; nPortrait < m_nCharacters; nPortrait++) {
        LONG nCharacterId = pGame->GetCharacterId(nPortrait);

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            if ((pSprite->m_baseStats.m_generalState & STATE_DEAD) == 0
                && (pSprite->m_derivedStats.m_generalState & STATE_DEAD) == 0) {
                pSprite->field_9D15 = 1;
            }

            pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// NOTE: Odd location.
//
// 0x428620
INT CInfGame::GetCurrentChapter()
{
    CVariable* pVar = GetVariables()->FindKey(CHAPTER_GLOBAL);
    if (pVar != NULL) {
        return pVar->m_intValue;
    }
    return 0;
}

// NOTE: Odd location
//
// 0x435110
void CInfGame::SetCurrentChapter(INT nChapter)
{
    CVariable* pVar = GetVariables()->FindKey(CHAPTER_GLOBAL);
    if (pVar != NULL) {
        pVar->m_intValue = nChapter;
    }
}

// 0x452FD0
CInfButtonArray* CInfGame::GetButtonArray()
{
    return &m_cButtonArray;
}

// 0x452FE0
LONG CInfGame::GetCharacterId(SHORT nPortrait)
{
    if (nPortrait < m_nCharacters) {
        return m_characterPortraits[nPortrait];
    }

    return CGameObjectArray::INVALID_INDEX;
}

// 0x453010
CTimerWorld* CInfGame::GetWorldTimer()
{
    return &m_worldTime;
}

// 0x453020
SHORT CInfGame::GetNumCharacters()
{
    return m_nCharacters;
}

// 0x453030
CGameObjectArray* CInfGame::GetObjectArray()
{
    return &m_cObjectArray;
}

// 0x453040
CGameArea* CInfGame::GetVisibleArea()
{
    return m_gameAreas[m_visibleArea];
}

// 0x453050
CGameSave* CInfGame::GetGameSave()
{
    return &m_gameSave;
}

// 0x453060
CMultiplayerSettings* CInfGame::GetMultiplayerSettings()
{
    return &m_multiplayerSettings;
}

// 0x439A70
CGameJournal* CInfGame::GetJournal()
{
    return &m_cJournal;
}

// 0x475DB0
CAIGroup* CInfGame::GetGroup()
{
    return &m_group;
}

// 0x4286A0
CVariableHash* CInfGame::GetVariables()
{
    return &m_variables;
}

// 0x6E4E80
CVidBitmap* CInfGame::GetMasterBitmap()
{
    return &m_rgbMasterBitmap;
}

// NOTE: Inlined.
LONG CInfGame::GetFixedOrderCharacterId(SHORT nSlot)
{
    if (nSlot < 6) {
        return m_characters[nSlot];
    }

    return CGameObjectArray::INVALID_INDEX;
}

// 0x40CAE0
CNamedCreatureVariableHash* CInfGame::GetNamedCreatures()
{
    return &m_namedCreatures;
}

// 0x68C010
void CInfGame::SetTempCursor(BYTE tempCursor)
{
    m_tempCursor = tempCursor;
}

// 0x45B630
CGameOptions* CInfGame::GetOptions()
{
    return &m_cOptions;
}

// 0x45B640
SHORT CInfGame::GetReputation()
{
    return m_nReputation / REPUTATION_MULTIPLIER;
}

// 0x45B660
CTypedPtrArray<CPtrArray, int*>& CInfGame::GetCharacterOverflow()
{
    return m_characterOverflow;
}

// 0x594070
SHORT CInfGame::GetState()
{
    return m_nState;
}

// 0x594080
void CInfGame::SetState(SHORT nState)
{
    m_nState = nState;
}

// 0x594090
BYTE CInfGame::GetIconIndex()
{
    return m_iconIndex;
}

// 0x5940A0
void CInfGame::SetIconIndex(BYTE iconIndex)
{
    m_iconIndex = iconIndex;
    m_iconResRef = "";
}

// 0x58FEC0
CSpellResRefList& CInfGame::GetMasterSongLookup()
{
    return m_songs;
}

// 0x5F67F0
CSpellResRefList* CInfGame::GetInnateSpells()
{
    return &m_innateSpells;
}

// 0x5F6800
CSpellResRefList* CInfGame::GetShapeshifts()
{
    return &m_shapeshifts;
}

// 0x64D660
CString CInfGame::GetSaveGame()
{
    return m_sSaveGame;
}

// 0x71AB80
void CInfGame::SetLastTarget(LONG id)
{
    m_lastClick.x = -1;
    m_lastClick.y = -1;
    m_lastTarget = id;
}

// NOTE: Inlined.
void CInfGame::SetLastClick(const CPoint& pt)
{
    m_lastClick = pt;
    m_lastTarget = CGameObjectArray::INVALID_INDEX;
}

// NOTE: Inlined.
BYTE CInfGame::GetKeymap(SHORT index)
{
    // __FILE__: .\Include\InfGame.h
    // __LINE__: 1486
    UTIL_ASSERT(index >= 0 && index < CINFGAME_KEYMAP_SIZE);

    return m_pKeymap[index];
}

// NOTE: Inlined.
BYTE CInfGame::GetKeymapFlag(SHORT index)
{
    // __FILE__: .\Include\InfGame.h
    // __LINE__: 1487
    UTIL_ASSERT(index >= 0 && index < CINFGAME_KEYMAP_SIZE);

    return m_pKeymapFlags[index];
}

// NOTE: Inlined
void CInfGame::SetQuickFormation(SHORT buttonNum, SHORT formation)
{
    // __FILE__: .\Include\InfGame.h
    // __LINE__: 1499
    UTIL_ASSERT(buttonNum < CGAMESAVE_NUM_QUICK_FORMATIONS);

    m_gameSave.m_quickFormations[buttonNum] = formation;
}

// NOTE: Inlined.
void CInfGame::SetFormationToQuickFormation(SHORT buttonNum)
{
    // __FILE__: .\Include\InfGame.h
    // __LINE__: 1500
    UTIL_ASSERT(buttonNum < CGAMESAVE_NUM_QUICK_FORMATIONS);

    m_gameSave.m_curFormation = m_gameSave.m_quickFormations[buttonNum];
}

// NOTE: Inlined.
void CInfGame::SetFormation(SHORT formation)
{
    m_gameSave.m_curFormation = formation;
}

// NOTE: Inlined.
BOOL CInfGame::IsFamiliar(LONG id)
{
    return m_familiars.Find(reinterpret_cast<int*>(id)) != NULL;
}

// NOTE: Inlined.
BOOL CInfGame::IsAlly(LONG id)
{
    return m_allies.Find(reinterpret_cast<int*>(id)) != NULL;
}

// -----------------------------------------------------------------------------

// NOTE: Inlined.
CSpellResRefList::CSpellResRefList()
{
    m_pList = NULL;
    m_nCount = 0;
}

// NOTE: Inlined.
CSpellResRefList::~CSpellResRefList()
{
    if (m_pList != NULL) {
        delete m_pList;
    }
    m_nCount = 0;
}

// FIXME: `nResRefColumn` should not be reference.
//
// 0x5C9FD0
void CSpellResRefList::Load(const C2DArray& Table, const INT& nResRefColumn)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 23354
    UTIL_ASSERT(Table.GetWidth() > 0);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 23355
    UTIL_ASSERT(Table.GetHeight() > 0);

    // FIXME: Uses unsigned compare.
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 23356
    UTIL_ASSERT(nResRefColumn < Table.GetWidth());

    if (m_pList != NULL) {
        delete m_pList;
        m_nCount = 0;
    }

    m_pList = new CResRef[Table.GetHeight()];

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 23369
    UTIL_ASSERT(m_pList != NULL);

    for (int index = 0; index < Table.GetHeight(); index++) {
        m_pList[index] = Table.GetAt(CPoint(nResRefColumn, index));
    }

    m_nCount = Table.GetHeight();
}

// 0x5CA160
bool CSpellResRefList::Find(const CResRef& resRef, UINT& nID)
{
    for (UINT nIndex = 0; nIndex < m_nCount; nIndex++) {
        if (m_pList[nIndex] == resRef) {
            nID = nIndex;
            return true;
        }
    }

    nID = 0;
    return false;
}

// NOTE: Inlined. Unclear why it's different from the one below.
const CResRef& CSpellResRefList::Get636(UINT nIndex)
{
    // __FILE__: .\Include\InfGame.h
    // __LINE__: 636
    UTIL_ASSERT(nIndex < m_nCount);

    // __FILE__: .\Include\InfGame.h
    // __LINE__: 637
    UTIL_ASSERT(m_pList != NULL);

    return m_pList[nIndex];
}

// FIXME: `nIndex` should not be reference.
// TODO: Is it `operator[]`?
//
// 0x5F6790
const CResRef& CSpellResRefList::Get(UINT& nIndex)
{
    // __FILE__: .\Include\InfGame.h
    // __LINE__: 652
    UTIL_ASSERT(nIndex < m_nCount);

    // __FILE__: .\Include\InfGame.h
    // __LINE__: 653
    UTIL_ASSERT(m_pList != NULL);

    return m_pList[nIndex];
}

// -----------------------------------------------------------------------------

// 0x59EB90
CSpellList::CSpellList()
{
    m_pList = NULL;
    m_nCount = 0;
}

// 0x59EBA0
CSpellList::~CSpellList()
{
    if (m_pList != NULL) {
        delete m_pList;
    }
    m_nCount = 0;
}

// 0x5CA1B0
void CSpellList::Load(const std::vector<CSpellListEntry>& spells)
{
    if (spells.empty()) {
        return;
    }

    size_t size = spells.size();
    if (size == 0) {
        return;
    }

    m_pList = new CSpellListEntry[size];

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 23430
    UTIL_ASSERT(m_pList != NULL);

    for (size_t index = 0; index < size; index++) {
        m_pList[index] = spells[index];
    }

    m_nCount = size;
}

// 0x5CA250
bool CSpellList::GetSpellLevel(const UINT& nID, UINT& nLevel)
{
    for (UINT nIndex = 0; nIndex < m_nCount; nIndex++) {
        if (m_pList[nIndex].m_nID == nID) {
            nLevel = m_pList[nIndex].m_nLevel;
            return true;
        }
    }

    nLevel = 0;
    return false;
}

// TODO: Is it `operator[]`?
//
// NOTE: Inlined.
UINT CSpellList::Get(UINT nIndex)
{
    // __FILE__: .\Include\InfGame.h
    // __LINE__: 724
    UTIL_ASSERT(nIndex < m_nCount);

    // __FILE__: .\Include\InfGame.h
    // __LINE__: 725
    UTIL_ASSERT(m_pList != NULL);

    return m_pList[nIndex].m_nID;
}

// -----------------------------------------------------------------------------

// 0x59EB60
CGroupedSpellList::CGroupedSpellList()
{
    m_nHighestLevel = 0;
}

// 0x59ECA0
CGroupedSpellList::~CGroupedSpellList()
{
}

// FIXME: `nClassColumn` should not be reference.
// FIXME: `nResRefColumn` should not be reference.
//
// 0x5CA2A0
void CGroupedSpellList::Load(const C2DArray& Table, const INT& nClassColumn, const INT& nResRefColumn, CSpellResRefList* pSpells)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 23495
    UTIL_ASSERT(Table.GetWidth() > 0);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 23496
    UTIL_ASSERT(Table.GetHeight() > 0);

    // FIXME: Uses unsigned compare.
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 23497
    UTIL_ASSERT(nClassColumn < Table.GetWidth());

    // FIXME: Uses unsigned compare.
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 23498
    UTIL_ASSERT(nResRefColumn < Table.GetWidth());

    std::vector<CSpellListEntry> lists[9];

    for (int index = 0; index < Table.GetHeight(); index++) {
        int nLevel = atol(Table.GetAt(CPoint(nClassColumn, index)));
        UINT nID = index;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
        // __LINE__: 23509
        UTIL_ASSERT(nLevel <= CSPELLLIST_MAX_LEVELS);

        CString sName = Table.GetAt(CPoint(nResRefColumn, index));
        CResRef resRef(sName);

        sName = sName.Left(strlen("**"));
        if (nLevel > 0 && sName != "**") {
            if (pSpells != NULL) {
                bool result = pSpells->Find(resRef, nID);

                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
                // __LINE__: 23524
                UTIL_ASSERT(result == true);
            }

            CSpellListEntry entry;
            entry.m_nID = nID;
            entry.m_nLevel = nLevel;
            lists[nLevel - 1].push_back(entry);
        }
    }

    // NOTE: Unsigned compare, implying `m_nHighestLevel` is also unsigned.
    for (UINT nLevel = 1; nLevel <= CSPELLLIST_MAX_LEVELS; nLevel++) {
        if (lists[nLevel - 1].size() != 0) {
            m_lists[nLevel - 1].Load(lists[nLevel - 1]);

            if (nLevel > m_nHighestLevel) {
                m_nHighestLevel = nLevel;
            }
        }
    }
}

// Raw "CRE V2.1" spell-block views read straight out of the old buffer.
// Their offsets are literal V2.1 file offsets from the exe; the V2.2 layout
// they are converted into is CCreatureFileOffsets (FileFormat.h).

// V2.1 flat memorized-spell entry: resref + spell level + list type
// (2 = song/shapeshift/innate). Stride 0xC.
struct COldCreatureFileSpell {
    BYTE m_resRef[8];
    SHORT m_spellLevel;
    SHORT m_type;
};
static_assert(sizeof(COldCreatureFileSpell) == 0xC, "V2.1 memorized-spell stride");

// V2.1 grouped known-spell header. Stride 0x10; m_startIndex indexes the
// shared known-spell pool (pool-wide, not per header).
struct COldCreatureFileSpellHeader {
    SHORT m_level;
    SHORT field_2;
    SHORT field_4;
    SHORT m_type;
    LONG m_startIndex;
    LONG m_count;
};
static_assert(sizeof(COldCreatureFileSpellHeader) == 0x10, "V2.1 spell-header stride");

// V2.1 known-spell pool entry: resref + flags (only bit 0 of m_flags is
// read — memorized). Stride 0xC.
struct COldCreatureFileKnownSpell {
    BYTE m_resRef[8];
    BYTE m_flags;
    BYTE field_9[3];
};
static_assert(sizeof(COldCreatureFileKnownSpell) == 0xC, "V2.1 known-spell stride");

// 0x5A3DB0
// UpgradeCreatureFileV20ToV21 — the "CRE V2.0" -> "CRE V2.1" step of the
// creature-file upgrade chain (0x5A7740 dispatches on the signature and
// calls this for "CRE V2.0"; 0x5A5060 then takes V2.1 -> V2.2). Builds the
// V2.1 image field by field in a temp buffer of *plSize bytes, then copies
// it back over *ppCreature — source and destination are the same caller
// buffer, which Unmarshal already grew by 0xCA bytes.
BOOL UpgradeCreatureFileV20ToV21(BYTE** ppCreature, LONG* plSize)
{
    BYTE* pOld = *ppCreature;

    BYTE* pNew = new BYTE[*plSize];
    memset(pNew, 0, *plSize);
    memcpy(pNew, "CRE V2.1", 8);

    // The exe zeroes the header block (0xDF dwords past the signature) a
    // second time before filling it; kept literal.
    memset(pNew + 8, 0, 0xDF * sizeof(DWORD));

    // First four header dwords move 1:1 (raw 0x8..0x14; the fourth is the
    // XP total the ladder below reads).
    memcpy(pNew + 0x8, pOld + 0x8, 0x10);

    // XP total -> level byte at V2.1 raw offset 0x25A (ebx runs from pNew+8
    // in the exe, so [ebx+0x252] is pNew+0x25A).
    DWORD nXp = *reinterpret_cast<const DWORD*>(pOld + 0x14);
    BYTE nLevel;
    if (nXp > 0xC350) {
        nLevel = 17;
    } else if (nXp > 0x7530) {
        nLevel = 16;
    } else if (nXp > 0x4E20) {
        nLevel = 15;
    } else if (nXp > 0x3A98) {
        nLevel = 14;
    } else if (nXp > 0x2EE0) {
        nLevel = 13;
    } else if (nXp > 0x2710) {
        nLevel = 12;
    } else if (nXp > 0x1F40) {
        nLevel = 11;
    } else if (nXp > 0x1770) {
        nLevel = 10;
    } else if (nXp > 0xFA0) {
        nLevel = 9;
    } else if (nXp > 0x7D0) {
        nLevel = 8;
    } else if (nXp > 0x578) {
        nLevel = 7;
    } else if (nXp > 0x1A4) {
        nLevel = 6;
    } else if (nXp > 0xAF) {
        nLevel = 5;
    } else if (nXp > 0x78) {
        nLevel = 4;
    } else if (nXp > 0x5A) {
        nLevel = 3;
    } else if (nXp > 0x3C) {
        nLevel = 2;
    } else if (nXp > 0) {
        nLevel = 1;
    } else {
        nLevel = 0;
    }
    pNew[0x25A] = nLevel;

    // Range 2 (0x5A3F52-0x5A41B9): header field moves. ebx = pNew+8 and
    // ebp = pOld in the exe, so [ebx+X] <- [ebp+X+8] is identity.

    // Identity block, raw 0x18..0x44 (exe walks it dword/word/byte).
    memcpy(pNew + 0x18, pOld + 0x18, 0x2D);

    // 0x48..0x56 compact by two (five words + five bytes).
    memcpy(pNew + 0x46, pOld + 0x48, 0xF);

    // 0x59..0x5c compact by four; 0x5d (signed) is divided by ten — the
    // imul 0x66666667 / sar 1 / round-toward-zero sequence at 0x5A4014.
    memcpy(pNew + 0x55, pOld + 0x59, 4);
    pNew[0x59] = static_cast<BYTE>(*reinterpret_cast<const signed char*>(pOld + 0x5D) / 10);
    memcpy(pNew + 0x5A, pOld + 0x5E, 6);

    // 0x6b..0x6d compact by six; 0x82 and the 0x84..0xa3 color block by
    // 0x1a (V2.1 re-rooms that region).
    memcpy(pNew + 0x65, pOld + 0x6B, 3);
    pNew[0x68] = pOld[0x82];
    memcpy(pNew + 0x6A, pOld + 0x84, 0x20);

    // 0xa4..0x1b7 grows by eight (rep movsd 0x40 dwords + five tail dwords).
    memcpy(pNew + 0xAC, pOld + 0xA4, 0x114);

    // Sparse moves into the V2.1 state block (0x220-0x242 region).
    pNew[0x258] = pOld[0x220];
    pNew[0x259] = pOld[0x221];
    memcpy(pNew + 0x25B, pOld + 0x224, 4);
    memcpy(pNew + 0x25F, pOld + 0x228, 4);
    pNew[0x263] = pOld[0x230];
    pNew[0x265] = pOld[0x237];
    pNew[0x266] = pOld[0x238];
    memcpy(pNew + 0x267, pOld + 0x23A, 7);
    memcpy(pNew + 0x26E, pOld + 0x242, 2);

    // Explicit zero stores in the exe (0x5A417F-0x5A41A4); already zero
    // from the fill, kept literal.
    memset(pNew + 0x218, 0, 6);

    // --- 0x5A41BA-0x5A4B9B: stat block and BG1 class conversion ---

    // Opening stat divides (0x5A41BA-0x5A427D): all unsigned /10 — each byte
    // loads into a zeroed cl (the xor ecx,ecx is scheduler-hoisted upstream:
    // 0x5A41D4 for src 0x68, 0x5A41EF for 0x67, 0x5A420A for 0x6A, 0x5A4216
    // for 0x69, 0x5A4247 for 0x231, 0x5A4256 for 0x83).
    pNew[0x21E] = pOld[0x45] / 10;
    pNew[0x220] = pOld[0x66]; // plain byte, no divide
    pNew[0x221] = pOld[0x68] / 10;
    pNew[0x222] = pOld[0x67] / 10;
    pNew[0x223] = pOld[0x6A] / 10;
    pNew[0x224] = pOld[0x69] / 10;
    pNew[0x225] = 0; // literal (0x5A422F)
    pNew[0x226] = pOld[0x231] / 10;
    pNew[0x227] = pOld[0x83] / 10;
    *reinterpret_cast<DWORD*>(pNew + 0x270) = 0; // xor edx,edx (0x5A4283)

    // Skills block (0x5A428B-0x5A4407): 0x2B bytes from 0x248 (ten dwords +
    // three bytes), 0x4A bytes from 0x274 (ten bytes + two rep movsd of
    // eight dwords), byte 0x2BE alone, then 0x18 bytes from 0x2C0. Src bytes
    // 0x273 and 0x2BF are dropped by the exe.
    memcpy(pNew + 0x274, pOld + 0x248, 0x2B);
    memcpy(pNew + 0x2A0, pOld + 0x274, 0x4A);
    pNew[0x2EA] = pOld[0x2BE];
    memcpy(pNew + 0x2EC, pOld + 0x2C0, 0x18);

    // Zero the V2.1 class/level block 0x384..0x3ED (rep stosd 0x1a + stosw,
    // 0x5A4415-0x5A441E; the mask dword at 0x3B6 is zeroed a second time at
    // 0x5A443A with the same edx still 0), then seed its first three bytes.
    memset(pNew + 0x384, 0, 0x6A);
    pNew[0x384] = pOld[0x2D8];
    pNew[0x385] = pOld[0x2D9];
    pNew[0x386] = pOld[0x2DA];

    // Normalize before dispatch (0x5A443D-0x5A446C): the V2.0 class byte at
    // 0x2DB is a BG1-era class.ids value; a blank 0/0xFF becomes FIGHTER(2),
    // and the level at 0x234 floors at 1. Both write back to the source.
    if (pOld[0x2DB] == 0 || pOld[0x2DB] == 0xFF) {
        pOld[0x2DB] = 2;
    }
    if (pOld[0x234] <= 1) {
        pOld[0x234] = 1;
    }

    // Dual-class arms halve the source levels and triple-class arms take
    // max(v/3,1); both write back before the level slots are read (the
    // and ecx,~1 / shr 1 and imul 0x55555556 idioms, unsigned byte loads
    // throughout — e.g. 0x5A4B32 and 0x5A4760).
    auto halveLevel = [&pOld](int nOffset) {
        pOld[nOffset] = pOld[nOffset] <= 3 ? 1 : pOld[nOffset] / 2;
    };
    auto splitLevelThirds = [&pOld](int nOffset) {
        BYTE nThirds = pOld[nOffset] / 3;
        pOld[nOffset] = nThirds > 1 ? nThirds : 1;
    };

    // Class dispatch (0x5A446A-0x5A448F) through the exe's jump tables at
    // 0x5A4F30/0x5A4F84. Case labels are BG1 class.ids values (1 MAGE,
    // 2 FIGHTER, 3 CLERIC, 4 THIEF, 5 BARD, 6 PALADIN, 7 F/M, 8 F/C, 9 F/T,
    // 10 F/M/T, 11 DRUID, 12 RANGER, 13 M/T, 14 C/M, 15 C/T, 16 F/D,
    // 17 F/M/C, 18 C/R); 202-209 are IWD2-beta ids aliasing the same arms.
    // Each arm sets the class-mask dword at 0x3B6 (CLASSMASK_*), the
    // primary-class byte at 0x387 (CAIOBJECTTYPE_C_*), and the per-class
    // level slots at 0x8B..0x95 — alphabetical: 0x8B barbarian, 0x8C bard,
    // 0x8D cleric, 0x8E druid, 0x8F fighter, 0x90 monk, 0x91 paladin,
    // 0x92 ranger, 0x93 rogue, 0x94 sorcerer, 0x95 wizard; 0x8A is the
    // combined level.
    BYTE levelSum = 0; // triple-class arms only, stored at 0x5A4C46
    switch (pOld[0x2DB]) {
    case 1: // MAGE
    case 202:
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_WIZARD; // 0x5A4496
        pNew[0x95] = pOld[0x234];
        pNew[0x387] = CAIOBJECTTYPE_C_WIZARD;
        pNew[0x8A] = pOld[0x234];
        *reinterpret_cast<DWORD*>(pNew + 0x270) = 0x4000; // kept literal
        break; // -> 0x5A4C4C

    case 2: // FIGHTER
    case 203: {
        // 0x5A44D4: tests the zero-extended WORD at 0x246 against
        // 0x40000000 — a bit a word load can never carry, so the barbarian
        // arm is dead in the exe; both paths ported as written.
        if ((*reinterpret_cast<const WORD*>(pOld + 0x246) & 0x40000000) != 0) {
            *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_BARBARIAN;
            pNew[0x8B] = pOld[0x234];
            pNew[0x387] = CAIOBJECTTYPE_C_BARBARIAN;
            pNew[0x8A] = pOld[0x234];
        } else {
            *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_FIGHTER;
            pNew[0x8F] = pOld[0x234];
            pNew[0x387] = CAIOBJECTTYPE_C_FIGHTER;
            pNew[0x8A] = pOld[0x234];
        }
        break; // -> 0x5A4C4C
    }

    case 3: // CLERIC
    case 204:
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_CLERIC; // 0x5A453E
        pNew[0x8D] = pOld[0x234];
        pNew[0x387] = CAIOBJECTTYPE_C_CLERIC;
        pNew[0x8A] = pOld[0x234];
        break; // -> 0x5A4C4C

    case 4: // THIEF
    case 205:
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_ROGUE; // 0x5A4570
        pNew[0x93] = pOld[0x234];
        pNew[0x387] = CAIOBJECTTYPE_C_ROGUE;
        pNew[0x8A] = pOld[0x234];
        break; // -> 0x5A4C4C

    case 5: // BARD
    case 206:
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_BARD; // 0x5A45A2
        pNew[0x8C] = pOld[0x234];
        pNew[0x387] = CAIOBJECTTYPE_C_BARD;
        pNew[0x8A] = pOld[0x234];
        break; // -> 0x5A4C4C

    case 6: // PALADIN
    case 207:
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_PALADIN; // 0x5A45D4
        pNew[0x91] = pOld[0x234];
        pNew[0x387] = CAIOBJECTTYPE_C_PALADIN;
        pNew[0x8A] = pOld[0x234];
        *reinterpret_cast<DWORD*>(pNew + 0x270) = 1; // kept literal
        break; // -> 0x5A4C4C

    case 7: // FIGHTER/MAGE
        halveLevel(0x234); // 0x5A4610
        halveLevel(0x235);
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_FIGHTER | CLASSMASK_WIZARD;
        pNew[0x8F] = pOld[0x234]; // 0x5A4665...
        pNew[0x387] = CAIOBJECTTYPE_C_FIGHTER;
        pNew[0x8F] = pOld[0x235]; // ...overwritten at 0x5A4675, kept literal
        pNew[0x8A] = static_cast<BYTE>(pOld[0x234] + pOld[0x235]); // 0x5A4BA0
        break;

    case 8: // FIGHTER/CLERIC
        halveLevel(0x234); // 0x5A4680
        halveLevel(0x235);
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_FIGHTER | CLASSMASK_CLERIC;
        pNew[0x8F] = pOld[0x234];
        pNew[0x8D] = pOld[0x235];
        pNew[0x387] = CAIOBJECTTYPE_C_FIGHTER;
        pNew[0x8A] = static_cast<BYTE>(pOld[0x234] + pOld[0x235]); // 0x5A4BA0
        break;

    case 9: // FIGHTER/THIEF
        halveLevel(0x234); // 0x5A46F0
        halveLevel(0x235);
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_FIGHTER | CLASSMASK_ROGUE;
        pNew[0x8F] = pOld[0x234];
        pNew[0x93] = pOld[0x235];
        pNew[0x387] = CAIOBJECTTYPE_C_FIGHTER;
        pNew[0x8A] = static_cast<BYTE>(pOld[0x234] + pOld[0x235]); // 0x5A4BA0
        break;

    case 10: // FIGHTER/MAGE/THIEF
        splitLevelThirds(0x234); // 0x5A4760
        splitLevelThirds(0x235);
        splitLevelThirds(0x236);
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) =
            CLASSMASK_FIGHTER | CLASSMASK_WIZARD | CLASSMASK_ROGUE;
        pNew[0x8F] = pOld[0x234];
        pNew[0x95] = pOld[0x235];
        pNew[0x93] = pOld[0x236];
        pNew[0x387] = CAIOBJECTTYPE_C_FIGHTER; // shared tail 0x5A4B13
        levelSum = static_cast<BYTE>(pOld[0x234] + pOld[0x236] + pOld[0x235]);
        pNew[0x8A] = levelSum; // 0x5A4C46
        break;

    case 11: // DRUID
    case 208:
        halveLevel(0x234); // 0x5A480E
        halveLevel(0x235);
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_DRUID;
        pNew[0x8E] = pOld[0x234];
        pNew[0x387] = CAIOBJECTTYPE_C_DRUID;
        pNew[0x8A] = pOld[0x234]; // 0x5A4C40
        break;

    case 12: // RANGER
    case 209:
        // 0x5A4872: no level math — the clamped level stays whole.
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_RANGER;
        pNew[0x92] = pOld[0x234];
        pNew[0x387] = CAIOBJECTTYPE_C_RANGER;
        pNew[0x8A] = pOld[0x234]; // 0x5A4C40
        break;

    case 13: // MAGE/THIEF
        halveLevel(0x234); // 0x5A4897
        halveLevel(0x235);
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_WIZARD | CLASSMASK_ROGUE;
        pNew[0x95] = pOld[0x234];
        pNew[0x93] = pOld[0x235];
        pNew[0x387] = CAIOBJECTTYPE_C_WIZARD;
        pNew[0x8A] = static_cast<BYTE>(pOld[0x234] + pOld[0x235]); // 0x5A4BA0
        break;

    case 14: // CLERIC/MAGE
        halveLevel(0x234); // 0x5A4907
        halveLevel(0x235);
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_CLERIC | CLASSMASK_WIZARD;
        pNew[0x8D] = pOld[0x234];
        pNew[0x95] = pOld[0x235];
        pNew[0x387] = CAIOBJECTTYPE_C_CLERIC; // 0x5A4B9C
        pNew[0x8A] = static_cast<BYTE>(pOld[0x234] + pOld[0x235]); // 0x5A4BA0
        break;

    case 15: // CLERIC/THIEF
        halveLevel(0x234); // 0x5A4976
        halveLevel(0x235);
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_CLERIC | CLASSMASK_ROGUE;
        pNew[0x8D] = pOld[0x234];
        pNew[0x93] = pOld[0x235];
        pNew[0x387] = CAIOBJECTTYPE_C_CLERIC; // 0x5A4B9C
        pNew[0x8A] = static_cast<BYTE>(pOld[0x234] + pOld[0x235]); // 0x5A4BA0
        break;

    case 16: // FIGHTER/DRUID
        halveLevel(0x234); // 0x5A49E5
        halveLevel(0x235);
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_FIGHTER | CLASSMASK_DRUID;
        pNew[0x8F] = pOld[0x234];
        pNew[0x8E] = pOld[0x235];
        pNew[0x387] = CAIOBJECTTYPE_C_FIGHTER;
        pNew[0x8A] = static_cast<BYTE>(pOld[0x234] + pOld[0x235]);
        break; // -> 0x5A4C4C

    case 17: // FIGHTER/MAGE/CLERIC
        splitLevelThirds(0x234); // 0x5A4A6A
        splitLevelThirds(0x235);
        splitLevelThirds(0x236);
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) =
            CLASSMASK_FIGHTER | CLASSMASK_WIZARD | CLASSMASK_CLERIC;
        pNew[0x8F] = pOld[0x234];
        pNew[0x95] = pOld[0x235];
        pNew[0x8D] = pOld[0x236];
        pNew[0x387] = CAIOBJECTTYPE_C_FIGHTER; // shared tail 0x5A4B13
        levelSum = static_cast<BYTE>(pOld[0x234] + pOld[0x236] + pOld[0x235]);
        pNew[0x8A] = levelSum; // 0x5A4C46
        break;

    case 18: // CLERIC/RANGER
        halveLevel(0x234); // 0x5A4B32
        halveLevel(0x235);
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_CLERIC | CLASSMASK_RANGER;
        pNew[0x8D] = pOld[0x234];
        pNew[0x92] = pOld[0x235];
        pNew[0x387] = CAIOBJECTTYPE_C_CLERIC; // 0x5A4B9C
        pNew[0x8A] = static_cast<BYTE>(pOld[0x234] + pOld[0x235]); // 0x5A4BA0
        break;

    case 19: // SORCERER, 0x5A4BB7
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_SORCERER; // 0x5A4BC1
        pNew[0x94] = pOld[0x234];
        pNew[0x387] = CAIOBJECTTYPE_C_SORCERER;
        pNew[0x8A] = pOld[0x234];
        break;

    case 20: // MONK, 0x5A4BE6
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_MONK; // 0x5A4BF0
        pNew[0x90] = pOld[0x234];
        pNew[0x8A] = pOld[0x234];
        pNew[0x387] = CAIOBJECTTYPE_C_MONK;
        *reinterpret_cast<DWORD*>(pNew + 0x270) = 8; // kit, kept literal
        break;

    default: // 21..201 and 210..255 land in the fighter arm, 0x5A4C1F
        *reinterpret_cast<DWORD*>(pNew + 0x3B6) = CLASSMASK_FIGHTER; // 0x5A4C29
        pNew[0x8F] = pOld[0x234];
        pNew[0x387] = CAIOBJECTTYPE_C_FIGHTER;
        pNew[0x8A] = pOld[0x234]; // 0x5A4C40
        break;
    }

    // --- 0x5A4C4C-0x5A4F2D: post-dispatch common tail ---

    // 0x5A4C50: mirror the primary class byte into the V2.1 copy.
    pNew[0x3B4] = pNew[0x387];
    // 0x5A4C5C-0x5A4CB3: 0x2C bytes, 0x2DC..0x307 -> 0x388..0x3B3.
    memcpy(pNew + 0x388, pOld + 0x2DC, 0x2C);

    // 0x5A4CB5-0x5A4D09: spellbook sub-header — 13 dwords from 0x308 land
    // at src+0xB2 (0x3BA..0x3EA). The six offset-valued dwords (0x308,
    // 0x310, 0x318, 0x320, 0x324, 0x32C) are re-based by the 0xB2 header
    // growth; the seven counts are copied unadjusted.
    *reinterpret_cast<DWORD*>(pNew + 0x3BA) =
        *reinterpret_cast<const DWORD*>(pOld + 0x308) + 0xB2;
    *reinterpret_cast<DWORD*>(pNew + 0x3BE) = *reinterpret_cast<const DWORD*>(pOld + 0x30C);
    *reinterpret_cast<DWORD*>(pNew + 0x3C2) =
        *reinterpret_cast<const DWORD*>(pOld + 0x310) + 0xB2;
    *reinterpret_cast<DWORD*>(pNew + 0x3C6) = *reinterpret_cast<const DWORD*>(pOld + 0x314);
    *reinterpret_cast<DWORD*>(pNew + 0x3CA) =
        *reinterpret_cast<const DWORD*>(pOld + 0x318) + 0xB2;
    *reinterpret_cast<DWORD*>(pNew + 0x3CE) = *reinterpret_cast<const DWORD*>(pOld + 0x31C);
    *reinterpret_cast<DWORD*>(pNew + 0x3D2) =
        *reinterpret_cast<const DWORD*>(pOld + 0x320) + 0xB2;
    *reinterpret_cast<DWORD*>(pNew + 0x3D6) =
        *reinterpret_cast<const DWORD*>(pOld + 0x324) + 0xB2;
    *reinterpret_cast<DWORD*>(pNew + 0x3DA) = *reinterpret_cast<const DWORD*>(pOld + 0x328);
    *reinterpret_cast<DWORD*>(pNew + 0x3DE) =
        *reinterpret_cast<const DWORD*>(pOld + 0x32C) + 0xB2;
    *reinterpret_cast<DWORD*>(pNew + 0x3E2) = *reinterpret_cast<const DWORD*>(pOld + 0x330);
    *reinterpret_cast<DWORD*>(pNew + 0x3E6) = *reinterpret_cast<const DWORD*>(pOld + 0x334);
    *reinterpret_cast<DWORD*>(pNew + 0x3EA) = *reinterpret_cast<const DWORD*>(pOld + 0x338);

    // 0x5A4D75-0x5A4DE0: memorization section, only when the three counts
    // at 0x30C/0x314/0x31C sum positive (the exe's test is signed). Its
    // byte size is recomputed from the counts — 12 per 0x30C entry, 12 per
    // spell named by each 0x10-byte record at pOld+pOld[0x310]+0xC (whose
    // first dword is itself a count), 16 per 0x314 record — and that many
    // bytes are copied verbatim from 0x33C to 0x3EE (= 0x33C + 0xB2).
    LONG nMemorizationCount = static_cast<LONG>(*reinterpret_cast<const DWORD*>(pOld + 0x30C)) +
        static_cast<LONG>(*reinterpret_cast<const DWORD*>(pOld + 0x314)) +
        static_cast<LONG>(*reinterpret_cast<const DWORD*>(pOld + 0x31C));
    if (nMemorizationCount > 0) {
        DWORD nRecords = *reinterpret_cast<const DWORD*>(pOld + 0x314);
        const BYTE* pRecord = pOld + *reinterpret_cast<const DWORD*>(pOld + 0x310) + 0xC;
        DWORD nBytes = 12 * *reinterpret_cast<const DWORD*>(pOld + 0x30C);
        for (DWORD nRecord = 0; nRecord < nRecords; ++nRecord, pRecord += 0x10) {
            nBytes += 12 * *reinterpret_cast<const DWORD*>(pRecord);
        }
        nBytes += 16 * nRecords;
        memcpy(pNew + 0x3EE, pOld + 0x33C, nBytes);
    }

    // 0x5A4DE2-0x5A4E22: item-slots section — 20 bytes per 0x328 entry,
    // copied verbatim from pOld+pOld[0x324] to pNew+pOld[0x324]+0xB2 (V2.1
    // slot pair 0x3D6/0x3DA).
    {
        DWORD nItems = *reinterpret_cast<const DWORD*>(pOld + 0x324);
        memcpy(pNew + nItems + 0xB2, pOld + nItems,
            20 * *reinterpret_cast<const DWORD*>(pOld + 0x328));
    }

    // 0x5A4E24-0x5A4E63: effects section, only when the 0x330 count is
    // nonzero — 48 bytes per entry, or 264 when the flag at pNew[0x33]
    // (copied from pOld[0x33]; the same last-block flag the V2.1->V2.2
    // upgrade reads) is set; from pOld+pOld[0x32C] to
    // pNew+pOld[0x32C]+0xB2 (V2.1 slot pair 0x3DE/0x3E2).
    if (*reinterpret_cast<const DWORD*>(pOld + 0x330) != 0) {
        DWORD nEffects = *reinterpret_cast<const DWORD*>(pOld + 0x32C);
        DWORD nBytes = (pNew[0x33] != 0 ? 264 : 48) * *reinterpret_cast<const DWORD*>(pOld + 0x330);
        memcpy(pNew + nEffects + 0xB2, pOld + nEffects, nBytes);
    }

    // 0x5A4E65-0x5A4EF8: equipment header (V2.1 slot 0x3D2). The V2.0
    // header at pOld+pOld[0x320] is rebuilt into the 0x68-byte V2.1 header
    // at pNew+pOld[0x320]+0xB2: 0x68 bytes first filled with 0xFF (rep
    // stosd of eax = -1, not zero — the empty-slot sentinel), then the
    // first 0x1A bytes copied 1:1, src 0x1A..0x4B shifted +8, and the two
    // words at 0x4C/0x4E shifted +0x18.
    {
        DWORD nEquip = *reinterpret_cast<const DWORD*>(pOld + 0x320);
        const BYTE* pEquip = pOld + nEquip;
        BYTE* pEquipNew = pNew + nEquip + 0xB2;
        memset(pEquipNew, 0xFF, 0x68);
        memcpy(pEquipNew, pEquip, 0x1A); // 0x5A4E75-0x5A4EC4
        memcpy(pEquipNew + 0x22, pEquip + 0x1A, 0x32); // +8, 0x5A4EC7-0x5A4EE6
        memcpy(pEquipNew + 0x64, pEquip + 0x4C, 4); // +0x18, 0x5A4EE8-0x5A4EF8
    }

    memcpy(*ppCreature, pNew, *plSize);
    delete[] pNew;
    return TRUE;
}

// 0x5A5060
BOOL UpgradeCreatureFileV21ToV22(BYTE** ppCreature, LONG* plSize, BYTE* pOldCreature, void* pUnused)
{
    BYTE* pOld = pOldCreature != NULL ? pOldCreature : *ppCreature;

    // The V2.1 class byte (raw offset 0x387) is normalized in the OLD buffer
    // before anything reads it: 0 or 0xFF (a raw byte compare) becomes 5.
    if (pOld[0x387] == 0 || pOld[0x387] == 0xFF) {
        pOld[0x387] = CAIOBJECTTYPE_C_FIGHTER;
    }

    UINT nClassIndex = 0;
    switch (pOld[0x387]) {
    case CAIOBJECTTYPE_C_BARD:
    case CAIOBJECTTYPE_C_CLERIC:
    case CAIOBJECTTYPE_C_DRUID:
    case CAIOBJECTTYPE_C_PALADIN:
    case CAIOBJECTTYPE_C_RANGER:
    case CAIOBJECTTYPE_C_SORCERER:
    case CAIOBJECTTYPE_C_WIZARD:
        nClassIndex = g_pBaldurChitin->GetObjectGame()->GetSpellcasterIndex(pOld[0x387]);
        break;
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameSpriteSpells spells;
    CGameSpriteGroupedSpellList domainSpells;
    CGameSpriteSpellList innateSpells;
    CGameSpriteSpellList songSpells;
    CGameSpriteSpellList shapeSpells;

    DWORD nOldSpellBytes = 0;

    // Loop 1 — the V2.1 flat memorized list (list pointer dword @0x3BA,
    // count dword @0x3BE).
    {
        COldCreatureFileSpell* pOldSpell = reinterpret_cast<COldCreatureFileSpell*>(pOld + *reinterpret_cast<DWORD*>(pOld + 0x3BA));
        DWORD nCount = *reinterpret_cast<DWORD*>(pOld + 0x3BE);
        nOldSpellBytes += nCount * sizeof(COldCreatureFileSpell);

        for (DWORD i = 0; i < nCount; i++) {
            COldCreatureFileSpell* tempOldSpell = pOldSpell + i;

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 5905
            UTIL_ASSERT(tempOldSpell->m_spellLevel >= 0 && tempOldSpell->m_spellLevel < CSPELLLIST_MAX_LEVELS);

            CResRef resRef(tempOldSpell->m_resRef);
            UINT nID = 0;
            const unsigned int one = 1;
            const unsigned int zero = 0;

            if (tempOldSpell->m_type == 2) { // 2: .rdata byte @0x85BB63
                // Song/shapeshift/innate spells are looked up in that order;
                // a hit lands in the matching extra list.
                if (pGame->m_songs.Find(resRef, nID)) {
                    songSpells.Add(nID, one, one, zero);
                } else if (pGame->m_shapeshifts.Find(resRef, nID)) {
                    shapeSpells.Add(nID, one, one, zero);
                } else if (pGame->m_innateSpells.Find(resRef, nID)) {
                    innateSpells.Add(nID, one, one, zero);
                } else {
                    // The exe formats this into a temporary CString and
                    // discards the result.
                    char sResRef[9];
                    memcpy(sResRef, tempOldSpell->m_resRef, 8);
                    sResRef[8] = '\0';
                    CString sObsolete(" ");
                    sObsolete.Format("*** OBSOLETE SPELL FILE: %s", sResRef);
                }
            } else {
                if (pGame->m_spells.Find(resRef, nID)) {
                    const UINT nLevel = static_cast<UINT>(static_cast<unsigned short>(tempOldSpell->m_spellLevel));
                    spells.Get(nClassIndex)->Add(nID, nLevel, one, one, zero);
                } else {
                    char sResRef[9];
                    memcpy(sResRef, tempOldSpell->m_resRef, 8);
                    sResRef[8] = '\0';
                    CString sObsolete(" ");
                    sObsolete.Format("*** OBSOLETE SPELL FILE: %s", sResRef);
                }
            }
        }
    }

    // Loop 2 — the V2.1 grouped known list (header pointer dword @0x3C2,
    // header count dword @0x3C6, shared entry pool dword @0x3CA).
    {
        COldCreatureFileSpellHeader* pHeader = reinterpret_cast<COldCreatureFileSpellHeader*>(pOld + *reinterpret_cast<DWORD*>(pOld + 0x3C2));
        DWORD nHeaderCount = *reinterpret_cast<DWORD*>(pOld + 0x3C6);
        COldCreatureFileKnownSpell* pPool = reinterpret_cast<COldCreatureFileKnownSpell*>(pOld + *reinterpret_cast<DWORD*>(pOld + 0x3CA));

        for (DWORD i = 0; i < nHeaderCount; i++, pHeader++) {
            nOldSpellBytes += sizeof(COldCreatureFileSpellHeader) + pHeader->m_count * sizeof(COldCreatureFileKnownSpell);

            for (LONG j = 0; j < pHeader->m_count; j++) {
                COldCreatureFileKnownSpell* pKnown = pPool + pHeader->m_startIndex + j;

                CResRef resRef(pKnown->m_resRef);
                UINT nID = 0;
                const unsigned int one = 1;
                const unsigned int zero = 0;
                const unsigned int nFlags = pKnown->m_flags & 1; // (flag & 1) ? 1 : 0

                if (pHeader->m_type == 2) { // 2: .rdata byte @0x85BB63
                    if (pGame->m_songs.Find(resRef, nID)) {
                        UINT nIndex = 0;
                        if (songSpells.Find(nID, nIndex)) {
                            songSpells.Add(nID, one, nFlags, zero);
                        }
                    } else if (pGame->m_shapeshifts.Find(resRef, nID)) {
                        UINT nIndex = 0;
                        if (shapeSpells.Find(nID, nIndex)) {
                            shapeSpells.Add(nID, one, nFlags, zero);
                        }
                    } else if (pGame->m_innateSpells.Find(resRef, nID)) {
                        UINT nIndex = 0;
                        if (innateSpells.Find(nID, nIndex)) {
                            innateSpells.Add(nID, one, nFlags, zero);
                        }
                    } else {
                        char sResRef[9];
                        memcpy(sResRef, pKnown->m_resRef, 8);
                        sResRef[8] = '\0';
                        CString sObsolete(" ");
                        sObsolete.Format("*** OBSOLETE SPELL FILE: %s", sResRef);
                    }
                } else {
                    if (pGame->m_spells.Find(resRef, nID)) {
                        const UINT nLevel = static_cast<UINT>(static_cast<unsigned short>(pHeader->m_level));
                        CGameSpriteGroupedSpellList* pClassSpells = spells.Get(nClassIndex);

                        UINT nIndex = 0;
                        if (pClassSpells->Find(nID, nLevel, nIndex)) {
                            pClassSpells->Add(nID, nLevel, one, nFlags, zero);
                        }

                        // The domain lookup runs regardless of the class
                        // lookup above. It cannot fire here (domainSpells
                        // starts empty and only this arm could fill it), but
                        // the exe performs it; ported as written.
                        nIndex = 0;
                        if (domainSpells.Find(nID, nLevel, nIndex)) {
                            domainSpells.Add(nID, nLevel, one, nFlags, zero);
                        }
                    } else {
                        char sResRef[9];
                        memcpy(sResRef, pKnown->m_resRef, 8);
                        sResRef[8] = '\0';
                        CString sObsolete(" ");
                        sObsolete.Format("*** OBSOLETE SPELL FILE: %s", sResRef);
                    }
                }
            }
        }
    }

    // Sizing — the V2.2 spell data is count x 0x10 + 8 per list (the +8 is
    // the trailing field_14/field_18 pair). nDelta relocates the tail.
    DWORD nTotalBytes = 0;
    for (UINT nClass = 0; nClass < CSPELLLIST_NUM_CLASSES; nClass++) {
        for (UINT nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
            nTotalBytes += static_cast<DWORD>(spells.Get(nClass)->GetSpellsAtLevel(nLevel)->m_List.size()) * sizeof(CCreatureFileSpell) + 8;
        }
    }
    for (UINT nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
        nTotalBytes += static_cast<DWORD>(domainSpells.GetSpellsAtLevel(nLevel)->m_List.size()) * sizeof(CCreatureFileSpell) + 8;
    }
    nTotalBytes += static_cast<DWORD>(innateSpells.m_List.size()) * sizeof(CCreatureFileSpell) + 8;
    nTotalBytes += static_cast<DWORD>(songSpells.m_List.size()) * sizeof(CCreatureFileSpell) + 8;
    nTotalBytes += static_cast<DWORD>(shapeSpells.m_List.size()) * sizeof(CCreatureFileSpell) + 8;

    const DWORD nDelta = nTotalBytes - nOldSpellBytes;

    // Build — the caller has already sized *plSize to fit the V2.2 image.
    BYTE* pNew = new BYTE[*plSize];
    memset(pNew, 0, *plSize);
    memcpy(pNew, "CRE V2.2", 8);

    // The V2.1 and V2.2 headers share 0xDF dwords past the signature (raw
    // offsets 8..0x383); the CCreatureFileOffsets region behind them stays
    // zero from the fill.
    memcpy(pNew + 8, pOld + 8, 0xDF * sizeof(DWORD));

    CCreatureFileOffsets* offsets = reinterpret_cast<CCreatureFileOffsets*>(pNew + 900);
    offsets->m_enemyAlly = pOld[0x384];
    offsets->m_general = pOld[0x385];
    offsets->m_race = pOld[0x386];
    // The exe re-checks the class byte here and this time normalizes 0/0xFF
    // to 2 — dead, the prologue above already rewrote it to 5; ported as
    // written.
    if (pOld[0x387] == 0 || pOld[0x387] == 0xFF) {
        pOld[0x387] = CAIOBJECTTYPE_C_BARD;
    }
    offsets->m_class = pOld[0x387];
    offsets->m_specifics = pOld[0x388];
    offsets->m_gender = pOld[0x389];
    memcpy(offsets->m_specialCase, pOld + 0x38A, 5);
    offsets->m_alignment = pOld[0x38F];
    offsets->m_instance = *reinterpret_cast<LONG*>(pOld + 0x390);
    memcpy(offsets->m_name, pOld + 0x394, SCRIPTNAME_SIZE);
    offsets->m_avClass = pOld[0x3B4];
    // (The exe stores zeroes at raw 0x3B6/0x3B8 before the classMask copy —
    // already zero here.)
    offsets->m_classMask = *reinterpret_cast<DWORD*>(pOld + 0x3B6);

    // Offsets table — every spell list gets its slot, empty or not; the
    // data cursor starts right behind the table.
    DWORD nRunning = 900 + sizeof(CCreatureFileOffsets);
    for (UINT nClass = 0; nClass < CSPELLLIST_NUM_CLASSES; nClass++) {
        for (UINT nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
            CGameSpriteSpellList* pList = spells.Get(nClass)->GetSpellsAtLevel(nLevel);

            offsets->m_spellListOffset[nClass][nLevel] = nRunning;
            offsets->m_spellListCount[nClass][nLevel] = static_cast<DWORD>(pList->m_List.size());
            nRunning += static_cast<DWORD>(pList->m_List.size()) * sizeof(CCreatureFileSpell) + 8;
        }
    }
    for (UINT nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
        CGameSpriteSpellList* pList = domainSpells.GetSpellsAtLevel(nLevel);

        offsets->m_domainListOffset[nLevel] = nRunning;
        offsets->m_domainListCount[nLevel] = static_cast<DWORD>(pList->m_List.size());
        nRunning += static_cast<DWORD>(pList->m_List.size()) * sizeof(CCreatureFileSpell) + 8;
    }
    offsets->m_innateListOffset = nRunning;
    offsets->m_innateListCount = static_cast<DWORD>(innateSpells.m_List.size());
    nRunning += static_cast<DWORD>(innateSpells.m_List.size()) * sizeof(CCreatureFileSpell) + 8;
    offsets->m_songListOffset = nRunning;
    offsets->m_songListCount = static_cast<DWORD>(songSpells.m_List.size());
    nRunning += static_cast<DWORD>(songSpells.m_List.size()) * sizeof(CCreatureFileSpell) + 8;
    offsets->m_shapeListOffset = nRunning;
    offsets->m_shapeListCount = static_cast<DWORD>(shapeSpells.m_List.size());
    nRunning += static_cast<DWORD>(shapeSpells.m_List.size()) * sizeof(CCreatureFileSpell) + 8;

    // The tail keeps its V2.1 offsets (item/equipment/effect pointers at raw
    // 0x3D2/0x3D6/0x3DE, counts at 0x3DA/0x3E2, dialog at 0x3E6), moved past
    // the grown spell data. Only the three offsets move; counts and the
    // dialog resref are copied verbatim.
    offsets->m_equipmentListOffset = *reinterpret_cast<DWORD*>(pOld + 0x3D2) + 0x240 + nDelta;
    offsets->m_itemListOffset = *reinterpret_cast<DWORD*>(pOld + 0x3D6) + 0x240 + nDelta;
    offsets->m_itemListCount = *reinterpret_cast<DWORD*>(pOld + 0x3DA);
    offsets->m_effectListOffset = *reinterpret_cast<DWORD*>(pOld + 0x3DE) + 0x240 + nDelta;
    offsets->m_effectListCount = *reinterpret_cast<DWORD*>(pOld + 0x3E2);
    memcpy(offsets->m_dialog, pOld + 0x3E6, sizeof(RESREF));

    // Copy — the lists land back to back from 0x62E; every list's entries
    // are followed by the field_14/field_18 pair, always written even when
    // the list is empty.
    DWORD nSpellOffset = 900 + sizeof(CCreatureFileOffsets);
    for (UINT nClass = 0; nClass < CSPELLLIST_NUM_CLASSES; nClass++) {
        for (UINT nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
            CGameSpriteSpellList* pList = spells.Get(nClass)->GetSpellsAtLevel(nLevel);

            for (UINT nIndex = 0; nIndex < offsets->m_spellListCount[nClass][nLevel]; nIndex++) {
                memcpy(pNew + nSpellOffset, pList->Get(nIndex), sizeof(CCreatureFileSpell));
                nSpellOffset += sizeof(CCreatureFileSpell);
            }

            *reinterpret_cast<unsigned int*>(pNew + nSpellOffset) = pList->field_14;
            *reinterpret_cast<unsigned int*>(pNew + nSpellOffset + 4) = pList->field_18;
            nSpellOffset += sizeof(unsigned int) + sizeof(unsigned int);
        }
    }
    for (UINT nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
        CGameSpriteSpellList* pList = domainSpells.GetSpellsAtLevel(nLevel);

        for (UINT nIndex = 0; nIndex < offsets->m_domainListCount[nLevel]; nIndex++) {
            memcpy(pNew + nSpellOffset, pList->Get(nIndex), sizeof(CCreatureFileSpell));
            nSpellOffset += sizeof(CCreatureFileSpell);
        }

        *reinterpret_cast<unsigned int*>(pNew + nSpellOffset) = pList->field_14;
        *reinterpret_cast<unsigned int*>(pNew + nSpellOffset + 4) = pList->field_18;
        nSpellOffset += sizeof(unsigned int) + sizeof(unsigned int);
    }
    for (UINT nIndex = 0; nIndex < offsets->m_innateListCount; nIndex++) {
        memcpy(pNew + nSpellOffset, innateSpells.Get(nIndex), sizeof(CCreatureFileSpell));
        nSpellOffset += sizeof(CCreatureFileSpell);
    }
    *reinterpret_cast<unsigned int*>(pNew + nSpellOffset) = innateSpells.field_14;
    *reinterpret_cast<unsigned int*>(pNew + nSpellOffset + 4) = innateSpells.field_18;
    nSpellOffset += sizeof(unsigned int) + sizeof(unsigned int);
    for (UINT nIndex = 0; nIndex < offsets->m_songListCount; nIndex++) {
        memcpy(pNew + nSpellOffset, songSpells.Get(nIndex), sizeof(CCreatureFileSpell));
        nSpellOffset += sizeof(CCreatureFileSpell);
    }
    *reinterpret_cast<unsigned int*>(pNew + nSpellOffset) = songSpells.field_14;
    *reinterpret_cast<unsigned int*>(pNew + nSpellOffset + 4) = songSpells.field_18;
    nSpellOffset += sizeof(unsigned int) + sizeof(unsigned int);
    for (UINT nIndex = 0; nIndex < offsets->m_shapeListCount; nIndex++) {
        memcpy(pNew + nSpellOffset, shapeSpells.Get(nIndex), sizeof(CCreatureFileSpell));
        nSpellOffset += sizeof(CCreatureFileSpell);
    }
    *reinterpret_cast<unsigned int*>(pNew + nSpellOffset) = shapeSpells.field_14;
    *reinterpret_cast<unsigned int*>(pNew + nSpellOffset + 4) = shapeSpells.field_18;
    nSpellOffset += sizeof(unsigned int) + sizeof(unsigned int);

    // The tail (equipment + items + effects) moves as one block. Its V2.1
    // size: 0x14 per item, 0x68 equipment, and 0x30 or 0x108 per effect
    // depending on the effect-size flag byte at raw 0x33 (carried into the
    // new header by the bulk copy above).
    DWORD nLastBlockSize = *reinterpret_cast<DWORD*>(pOld + 0x3DA) * sizeof(CCreatureFileItem) + sizeof(CCreatureFileEquipment) + *reinterpret_cast<DWORD*>(pOld + 0x3E2) * (pNew[0x33] == 0 ? 0x30 : 0x108);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 6423
    UTIL_ASSERT((nSpellOffset + nLastBlockSize) == static_cast<DWORD>(*plSize));

    memcpy(pNew + nSpellOffset, pOld + 0x3EE + nOldSpellBytes, nLastBlockSize);
    memcpy(*ppCreature, pNew, *plSize);
    delete[] pNew;

    return TRUE;
}

// 0x5A69B0
LONG ComputeCreatureFileV21ToV22Delta(BYTE* pOldCreature)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
    // __LINE__: 6440
    UTIL_ASSERT(pOldCreature != NULL);

    // Version gate — two 2-dword compares, "CRE V2.0" then "CRE V2.1";
    // either passes. The only caller (0x70D0FB, the "CRE V2.1" arm of
    // CGameSprite::Unmarshal) always arrives with V2.1 — the V2.0 arm of
    // that dispatch routes through UpgradeCreatureFileV20ToV21 first — so
    // the V2.0 half is defensive. Anything else asserts with a literal
    // FALSE and the delta is 0 (the caller then grows the buffer by exactly
    // 0x240).
    if (memcmp(pOldCreature, "CRE V2.0", 8) != 0 && memcmp(pOldCreature, "CRE V2.1", 8) != 0) {
        // __LINE__: 6446
        UTIL_ASSERT(FALSE);
        return 0;
    }

    // The V2.1 class byte (raw offset 0x387) is normalized in the OLD buffer
    // before anything reads it: 0 or 0xFF (a raw byte compare) becomes 5 —
    // the same normalization UpgradeCreatureFileV21ToV22 performs on its
    // source image.
    if (pOldCreature[0x387] == 0 || pOldCreature[0x387] == 0xFF) {
        pOldCreature[0x387] = CAIOBJECTTYPE_C_FIGHTER;
    }

    UINT nClassIndex = 0;
    switch (pOldCreature[0x387]) {
    case CAIOBJECTTYPE_C_BARD:
    case CAIOBJECTTYPE_C_CLERIC:
    case CAIOBJECTTYPE_C_DRUID:
    case CAIOBJECTTYPE_C_PALADIN:
    case CAIOBJECTTYPE_C_RANGER:
    case CAIOBJECTTYPE_C_SORCERER:
    case CAIOBJECTTYPE_C_WIZARD:
        nClassIndex = g_pBaldurChitin->GetObjectGame()->GetSpellcasterIndex(pOldCreature[0x387]);
        break;
    }

    // The rebuild lists — the same locals UpgradeCreatureFileV21ToV22 fills:
    // this helper duplicates the list-building walk to measure what the V2.2
    // spell data will weigh (the exe carries the same duplication).
    CGameSpriteSpells spells;
    CGameSpriteGroupedSpellList domainSpells;
    CGameSpriteSpellList innateSpells;
    CGameSpriteSpellList songSpells;
    CGameSpriteSpellList shapeSpells;

    DWORD nOldSpellBytes = 0;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // Loop 1 — the V2.1 flat memorized list (list pointer dword @0x3BA,
    // count dword @0x3BE). The exe adds the flat block's 0xC-per-entry
    // bytes after the loop (0x5A6ED1); same sum here.
    {
        COldCreatureFileSpell* pOldSpell = reinterpret_cast<COldCreatureFileSpell*>(pOldCreature + *reinterpret_cast<DWORD*>(pOldCreature + 0x3BA));
        DWORD nCount = *reinterpret_cast<DWORD*>(pOldCreature + 0x3BE);
        nOldSpellBytes += nCount * sizeof(COldCreatureFileSpell);

        for (DWORD i = 0; i < nCount; i++) {
            COldCreatureFileSpell* tempOldSpell = pOldSpell + i;

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 6486
            UTIL_ASSERT(tempOldSpell->m_spellLevel >= 0 && tempOldSpell->m_spellLevel < CSPELLLIST_MAX_LEVELS);

            CResRef resRef(tempOldSpell->m_resRef);
            UINT nID = 0;
            const unsigned int one = 1;
            const unsigned int zero = 0;

            if (tempOldSpell->m_type == 2) { // 2: .rdata byte @0x85BB63
                // Song/shapeshift/innate spells are looked up in that order;
                // a hit lands in the matching extra list.
                if (pGame->m_songs.Find(resRef, nID)) {
                    songSpells.Add(nID, one, one, zero);
                } else if (pGame->m_shapeshifts.Find(resRef, nID)) {
                    shapeSpells.Add(nID, one, one, zero);
                } else if (pGame->m_innateSpells.Find(resRef, nID)) {
                    innateSpells.Add(nID, one, one, zero);
                } else {
                    // The exe formats this into a temporary CString and
                    // discards the result.
                    char sResRef[9];
                    memcpy(sResRef, tempOldSpell->m_resRef, 8);
                    sResRef[8] = '\0';
                    CString sObsolete(" ");
                    sObsolete.Format("*** OBSOLETE SPELL FILE: %s", sResRef);
                }
            } else {
                if (pGame->m_spells.Find(resRef, nID)) {
                    const UINT nLevel = static_cast<UINT>(static_cast<unsigned short>(tempOldSpell->m_spellLevel));
                    spells.Get(nClassIndex)->Add(nID, nLevel, one, one, zero);
                } else {
                    char sResRef[9];
                    memcpy(sResRef, tempOldSpell->m_resRef, 8);
                    sResRef[8] = '\0';
                    CString sObsolete(" ");
                    sObsolete.Format("*** OBSOLETE SPELL FILE: %s", sResRef);
                }
            }
        }
    }

    // TODO(range 3, 0x5A6ED1-0x5A7461): loop 2 — the known-spell headers.
    // TODO(range 4, 0x5A7461-0x5A7731): the sizing walk and the delta.

    return 0;
}

// -----------------------------------------------------------------------------

// 0x5C7AC0
BYTE* CGameFile::GetData()
{
    if (pRes != NULL) {
        return reinterpret_cast<BYTE*>(pRes->Demand());
    } else {
        return NULL;
    }
}

// 0x5C7AD0
DWORD CGameFile::GetDataSize()
{
    if (pRes == NULL) {
        return 0;
    }

    if (pRes->Demand() == NULL) {
        return 0;
    }

    DWORD nSize = pRes->m_nResSizeActual;

    pRes->Release();

    return nSize;
}

// 0x5B7850
BOOLEAN CInfGame::SwapItemGround(LONG nContainerId, SHORT nSlotNum, CItem*& pItem, STRREF& errorCode, WORD wCount, BOOLEAN bFromServer)
{
    CGameContainer* pContainer;

    // NOTE: A single attempt, as in the original.
    BYTE rc = GetObjectArray()->GetDeny(nContainerId,
        CGameObjectArray::THREAD_ASYNCH,
        reinterpret_cast<CGameObject**>(&pContainer),
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    // NOTE: A client sends a swap request to the host here
    // (CBaldurMessage 0x431560); multiplayer is removed (ADR 0002).

    CItem* pGroundItem = pContainer->GetItem(nSlotNum);
    if (pGroundItem != NULL) {
        if (pItem == NULL
            && pGroundItem->GetMaxStackable() > 1
            && wCount < pGroundItem->GetUsageCount(0)) {
            // Take part of the stack.

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 12276
            UTIL_ASSERT(wCount != 0);

            pGroundItem->SetUsageCount(0, pGroundItem->GetUsageCount(0) - wCount);

            pItem = new CItem(pGroundItem->cResRef, 0, 0, 0, 0, 0);
            pItem->SetUsageCount(0, wCount);
            pItem->m_flags = (pItem->m_flags & ~0x1) | (pGroundItem->m_flags & 0x1);

            GetObjectArray()->ReleaseDeny(nContainerId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            errorCode = -1;
            return TRUE;
        }

        if (pItem != NULL
            && pGroundItem->GetMaxStackable() > 1
            && pItem->cResRef == pGroundItem->cResRef
            && ((pItem->m_flags ^ pGroundItem->m_flags) & 0x1) == 0) {
            // Add to the stack.

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 12286
            UTIL_ASSERT(wCount != 0);

            INT nSpace;
            if (pItem->GetUsageCount(0) < pGroundItem->GetMaxStackable() - pGroundItem->GetUsageCount(0)) {
                nSpace = pItem->GetUsageCount(0);
            } else {
                nSpace = pGroundItem->GetMaxStackable() - pGroundItem->GetUsageCount(0);
            }

            if (wCount >= nSpace) {
                if (pItem->GetUsageCount(0) < pGroundItem->GetMaxStackable() - pGroundItem->GetUsageCount(0)) {
                    wCount = pItem->GetUsageCount(0);
                } else {
                    wCount = pGroundItem->GetMaxStackable() - pGroundItem->GetUsageCount(0);
                }
            }

            pGroundItem->SetUsageCount(0, pGroundItem->GetUsageCount(0) + wCount);

            if (pItem->GetUsageCount(0) == wCount) {
                // NOTE: Uninline.
                g_pBaldurChitin->GetObjectGame()->AddDisposableItem(pItem);
                pItem = NULL;
            } else {
                pItem->SetUsageCount(0, pItem->GetUsageCount(0) - wCount);
            }

            GetObjectArray()->ReleaseDeny(nContainerId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            errorCode = -1;
            return TRUE;
        }
    }

    if (pItem == NULL) {
        // Pick the item up.
        pContainer->SetItem(nSlotNum, NULL);

        CMessageContainerAddItem* pMessage = new CMessageContainerAddItem(CItem(),
            nSlotNum,
            FALSE,
            pContainer->GetId(),
            pContainer->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

        pItem = pGroundItem;
    } else if (pGroundItem != NULL && pGroundItem->cResRef == "MISC07") {
        // Gold on the ground goes to the party; the item stays in hand.
        CInfGame* pInfGame = g_pBaldurChitin->GetObjectGame();
        pInfGame->AddPartyGold(pGroundItem->GetUsageCount(0));

        // NOTE: Uninline.
        pInfGame->AddDisposableItem(pGroundItem);

        pContainer->SetItem(nSlotNum, NULL);

        CMessageContainerAddItem* pMessage = new CMessageContainerAddItem(CItem(),
            nSlotNum,
            FALSE,
            pContainer->GetId(),
            pContainer->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    } else {
        // Put the item down, taking whatever lay there. The slot is set by
        // the message from a copy of the item.
        CMessageContainerAddItem* pMessage = new CMessageContainerAddItem(*pItem,
            nSlotNum,
            FALSE,
            pContainer->GetId(),
            pContainer->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

        // NOTE: Uninline.
        g_pBaldurChitin->GetObjectGame()->AddDisposableItem(pItem);

        pItem = pGroundItem;
    }

    GetObjectArray()->ReleaseDeny(nContainerId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
    errorCode = -1;
    return TRUE;
}

// 0x5B81C0
BOOL CInfGame::SwapItemPersonal(SHORT nPortrait, SHORT nSlotNum, CItem*& pItem, STRREF& errorCode, WORD wCount, BOOLEAN bFromServer)
{
    // NOTE: Uninline.
    LONG nCharacterId = GetCharacterId(nPortrait);

    CItem* pNewItem = pItem;
    BOOL bResult = TRUE;
    BOOL bEquip = TRUE;
    BOOL bQuickItemSlot = FALSE;

    // NOTE: The original reads an uninitialized local when no item is held;
    // bounded here (P4-10) with a type no slot check matches.
    SHORT nType = 0;

    if (pNewItem != NULL) {
        nType = pNewItem->GetItemType();
    }

    errorCode = -1;

    if (nSlotNum >= CGameSpriteEquipment::NUM_SLOT) {
        errorCode = 9374;
        return FALSE;
    }

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        errorCode = 9373;
        return FALSE;
    }

    if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
        && g_pChitin->cNetwork.m_idLocalPlayer != pSprite->m_remotePlayerID) {
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        errorCode = 20695;
        return FALSE;
    }

    CAIObjectType typeAI;
    typeAI.Set(pSprite->GetAIType());

    // `bDone` jumps to the final release, as the original's shared exit.
    BOOL bDone = FALSE;

    if (pItem != NULL) {
        DWORD nNotUsableBy = pItem->GetNotUsableBy();

        // NOTE: The original inlines the alignment checks of
        // CRuleTables::IsUsableByAlignment in another order.
        if (!g_pBaldurChitin->GetObjectGame()->GetRuleTables().IsUsableByAlignment(nNotUsableBy, typeAI.m_nAlignment)) {
            bResult = FALSE;
            errorCode = 9382;
        }

        if ((nNotUsableBy & 0x800000) != 0 && typeAI.m_nRace == CAIObjectType::R_ELF) {
            bResult = FALSE;
            errorCode = 9382;
        }

        if ((nNotUsableBy & 0x1000000) != 0 && typeAI.m_nRace == CAIObjectType::R_DWARF) {
            bResult = FALSE;
            errorCode = 9382;
        }

        if ((nNotUsableBy & 0x2000000) != 0 && typeAI.m_nRace == CAIObjectType::R_HALF_ELF) {
            bResult = FALSE;
            errorCode = 9382;
        }

        if ((nNotUsableBy & 0x4000000) != 0 && typeAI.m_nRace == CAIObjectType::R_HALFLING) {
            bResult = FALSE;
            errorCode = 9382;
        }

        if ((nNotUsableBy & 0x8000000) != 0 && typeAI.m_nRace == CAIObjectType::R_HUMAN) {
            bResult = FALSE;
            errorCode = 9382;
        }

        if ((nNotUsableBy & 0x10000000) != 0 && typeAI.m_nRace == CAIObjectType::R_GNOME) {
            bResult = FALSE;
            errorCode = 9382;
        }

        if ((nNotUsableBy & 0x20000000) != 0 && typeAI.m_nRace == CAIObjectType::R_HALF_ORC) {
            bResult = FALSE;
            errorCode = 9382;
        }

        if (!bResult) {
            bDone = TRUE;
        } else if (g_pBaldurChitin->GetObjectGame()->GetRuleTables().ShouldCheckItemRequirements(pItem)) {
            CDerivedStats& stats = pSprite->m_derivedStats;

            if (stats.m_nLevel < pItem->GetMinLevelRequired()) {
                bResult = FALSE;
                errorCode = 9382;
            }

            if (stats.m_nSTR < static_cast<SHORT>(pItem->GetMinSTRRequired())) {
                bResult = FALSE;
                errorCode = 9382;
            }

            if (stats.m_nINT < static_cast<SHORT>(pItem->GetMinINTRequired())) {
                bResult = FALSE;
                errorCode = 9382;
            }

            if (stats.m_nDEX < static_cast<SHORT>(pItem->GetMinDEXRequired())) {
                bResult = FALSE;
                errorCode = 9382;
            }

            if (stats.m_nWIS < static_cast<SHORT>(pItem->GetMinWISRequired())) {
                bResult = FALSE;
                errorCode = 9382;
            }

            if (stats.m_nCON < static_cast<SHORT>(pItem->GetMinCONRequired())) {
                bResult = FALSE;
                errorCode = 9382;
            }

            if (stats.m_nCHR < static_cast<SHORT>(pItem->GetMinCHRRequired())) {
                bResult = FALSE;
                errorCode = 9382;
            }

            if (!bResult) {
                bDone = TRUE;
            }
        }
    }

    if (!bDone) {
        // Can the slot take the item?
        INT nSlot = nSlotNum;
        CItem** pSlots = pSprite->m_equipment.m_items;

        switch (nSlot) {
        case 0:
            if (pItem != NULL && nType != 1 && nType != 70) {
                errorCode = 9375;
                bResult = FALSE;
                bDone = TRUE;
            }
            break;
        case 1:
            if (pItem != NULL
                && nType != 60
                && nType != 61
                && nType != 66
                && nType != 62
                && nType != 63
                && nType != 64
                && nType != 65
                && nType != 67
                && nType != 68) {
                errorCode = 9375;
                bResult = FALSE;
                bDone = TRUE;
            }
            break;
        case 2:
            if (pItem != NULL && nType != 3) {
                errorCode = 9375;
                bResult = FALSE;
                bDone = TRUE;
            }
            break;
        case 3:
            if (pItem != NULL && nType != 4) {
                errorCode = 9375;
                bResult = FALSE;
                bDone = TRUE;
            }
            break;
        case 4:
            if (pItem != NULL && nType != 32) {
                errorCode = 9375;
                bResult = FALSE;
                bDone = TRUE;
            }
            break;
        case 5:
            if (pItem != NULL && nType != 6 && nType != 73) {
                errorCode = 9375;
                bResult = FALSE;
                bDone = TRUE;
            }
            break;
        case 6:
            if (pItem != NULL && nType != 7 && nType != 72) {
                errorCode = 9375;
                bResult = FALSE;
                bDone = TRUE;
            }
            break;
        case 7:
        case 8:
            if (pItem != NULL && nType != 10) {
                errorCode = 9375;
                bResult = FALSE;
                bDone = TRUE;
            }
            break;
        case 10:
            errorCode = 9375;
            bResult = FALSE;
            bDone = TRUE;
            break;
        case 11:
        case 12:
        case 13:
        case 14:
            // Quivers.
            if (pItem != NULL && nType != 5 && nType != 31 && nType != 14) {
                bResult = FALSE;
                errorCode = 9375;
            }
            bEquip = FALSE;
            break;
        case 15:
        case 16:
        case 17:
            // Quick items.
            bQuickItemSlot = TRUE;
            if (pItem != NULL) {
                if (nType != 0
                    && nType != 8
                    && nType != 9
                    && nType != 11
                    && nType != 35
                    && nType != 71) {
                    bResult = FALSE;
                    errorCode = 9375;
                    bEquip = FALSE;
                    break;
                }

                if (pItem->cResRef == "SPWI110Z") {
                    errorCode = 9375;
                    bResult = FALSE;
                } else if (pItem->cResRef == "MISC86") {
                    errorCode = 9375;
                    bResult = FALSE;
                } else if (pItem->GetAbilityCount() == 0) {
                    errorCode = 9375;
                    bResult = FALSE;
                } else if (g_pBaldurChitin->GetObjectGame()->CheckItemUsable(pSprite, pItem) == 0) {
                    errorCode = 9382;
                    bResult = FALSE;
                }
            }
            bEquip = FALSE;
            break;
        case 43:
        case 45:
        case 47:
        case 49:
            // Main hand.
            if (pSprite->m_nWeaponSet != (nSlot - 43) / 2) {
                bEquip = FALSE;
            }

            if (pItem != NULL
                && nType != 15
                && nType != 16
                && nType != 17
                && nType != 44
                && nType != 18
                && nType != 19
                && nType != 20
                && nType != 57
                && nType != 69
                && nType != 21
                && nType != 22
                && nType != 23
                && nType != 24
                && nType != 25
                && nType != 26
                && nType != 27
                && nType != 29
                && nType != 30) {
                bResult = FALSE;
                errorCode = 9375;
            }

            if (nType != 15 && nType != 27 && nType != 18) {
                // A melee weapon: no ranged ability beside an off-hand
                // weapon, no two-handed weapon beside anything.
                CItem* pOffHand = pSlots[nSlot + 1];
                if (pOffHand != NULL
                    && pOffHand->GetItemType() != 47
                    && pOffHand->GetItemType() != 53
                    && pOffHand->GetItemType() != 49
                    && pOffHand->GetItemType() != 41
                    && pItem != NULL) {
                    pItem->Demand();
                    for (INT nAbility = 0; nAbility < pItem->GetAbilityCount() && bResult; nAbility++) {
                        ITEM_ABILITY* pAbility = pItem->GetAbility(nAbility);
                        if (pAbility != NULL) {
                            if ((pAbility->type & 0xFF) == 2) {
                                errorCode = 28765;
                                bResult = FALSE;
                            } else if ((pAbility->type & 0xFF) == 4) {
                                errorCode = 28766;
                                bResult = FALSE;
                            }
                        }
                    }
                    pItem->Release();

                    if (!bResult) {
                        bDone = TRUE;
                        break;
                    }
                }

                if (pItem != NULL
                    && (pItem->GetFlagsFile() & 0x2) != 0
                    && pSlots[nSlot + 1] != NULL) {
                    if (pSlots[nSlot + 1]->GetItemType() == 47
                        || pSlots[nSlot + 1]->GetItemType() == 53
                        || pSlots[nSlot + 1]->GetItemType() == 49
                        || pSlots[nSlot + 1]->GetItemType() == 41) {
                        errorCode = 9380;
                    } else {
                        errorCode = 28758;
                    }
                    bResult = FALSE;
                    bDone = TRUE;
                }
                break;
            }

            // A launcher: nothing in the off hand but a shield, and a shield
            // only beside a sling.
            if (pSlots[nSlot + 1] != NULL) {
                if (pSlots[nSlot + 1]->GetItemType() != 47
                    && pSlots[nSlot + 1]->GetItemType() != 53
                    && pSlots[nSlot + 1]->GetItemType() != 49
                    && pSlots[nSlot + 1]->GetItemType() != 41) {
                    errorCode = 28765;
                    bResult = FALSE;
                    bDone = TRUE;
                    break;
                }

                if (nType != 18) {
                    bResult = FALSE;
                    errorCode = 9380;
                }
            }
            bEquip = FALSE;
            break;
        case 44:
        case 46:
        case 48:
        case 50:
            // Off hand.
            if (pSprite->m_nWeaponSet != (nSlot - 43) / 2) {
                bEquip = FALSE;
            }

            if (pItem != NULL) {
                if (nType == 16
                    || nType == 17
                    || nType == 44
                    || nType == 18
                    || nType == 19
                    || nType == 20
                    || nType == 69
                    || nType == 57
                    || nType == 21
                    || nType == 22
                    || nType == 23
                    || nType == 24
                    || nType == 25
                    || nType == 26
                    || nType == 27
                    || nType == 29
                    || nType == 30) {
                    if ((pItem->GetFlagsFile() & 0x2) != 0) {
                        errorCode = 26342;
                        bResult = FALSE;
                        bDone = TRUE;
                        break;
                    }

                    pItem->Demand();
                    for (INT nAbility = 0; nAbility < pItem->GetAbilityCount() && bResult; nAbility++) {
                        ITEM_ABILITY* pAbility = pItem->GetAbility(nAbility);
                        if (pAbility != NULL
                            && ((pAbility->type & 0xFF) == 2 || (pAbility->type & 0xFF) == 4)) {
                            errorCode = 1314;
                            bResult = FALSE;
                        }
                    }
                    pItem->Release();
                } else if (nType != 47 && nType != 49 && nType != 53 && nType != 41) {
                    errorCode = 9375;
                    bResult = FALSE;
                    bDone = TRUE;
                    break;
                }
            }

            if (pSlots[42] != NULL
                && (pSlots[42]->GetFlagsFile() & 0x2) != 0
                && pItem != NULL) {
                errorCode = 9381;
                bResult = FALSE;
                bDone = TRUE;
                break;
            }

            if (pSlots[nSlot - 1] != NULL && pItem != NULL) {
                CItem* pMainHand = pSlots[nSlot - 1];
                if ((pMainHand->GetFlagsFile() & 0x2) != 0
                    && pMainHand != pSlots[CGameSpriteEquipment::SLOT_FIST]) {
                    errorCode = 9381;
                    bResult = FALSE;
                    bDone = TRUE;
                    break;
                }

                if (nType != 47 && nType != 53 && nType != 49 && nType != 41) {
                    pMainHand->Demand();
                    for (INT nAbility = 0; nAbility < pMainHand->GetAbilityCount() && bResult; nAbility++) {
                        ITEM_ABILITY* pAbility = pMainHand->GetAbility(nAbility);
                        if (pAbility != NULL) {
                            if ((pAbility->type & 0xFF) == 2) {
                                errorCode = 1310;
                                bResult = FALSE;
                            } else if ((pAbility->type & 0xFF) == 4) {
                                errorCode = 1311;
                                bResult = FALSE;
                            }
                        }
                    }
                    pMainHand->Release();
                }
            }
            break;
        default:
            bEquip = FALSE;
            break;
        }
    }

    if (!bDone && bResult) {
        // Immunities to the item and its type.
        if (pNewItem != NULL) {
            STRREF strError;
            CGameEffect* pEffect;
            if (pSprite->m_derivedStats.m_cImmunitiesItemEquip.OnList(pNewItem->cResRef, strError, pEffect)) {
                bResult = FALSE;
                errorCode = strError;
                if (pEffect != NULL) {
                    delete pEffect;
                }
                bDone = TRUE;
            } else if (pSprite->m_derivedStats.m_cImmunitiesItemTypeEquip.OnList(pNewItem->GetItemType(), strError, pEffect)) {
                bResult = FALSE;
                errorCode = strError;
                if (pEffect != NULL) {
                    delete pEffect;
                }
                bDone = TRUE;
            }
        }
    }

    if (!bDone && bResult) {
        CItem** pSlots = pSprite->m_equipment.m_items;

        // A created weapon holds the weapon and quiver slots.
        if (pSlots[42] != NULL
            && ((nSlotNum >= 43 && nSlotNum <= 50) || (nSlotNum >= 11 && nSlotNum <= 14))) {
            errorCode = 10141;
            bResult = FALSE;
            bDone = TRUE;
        } else if (pItem != NULL && IsItemExclusive(pSprite, nSlotNum, pItem, errorCode)) {
            bResult = FALSE;
            bDone = TRUE;
        } else {
            if (pSlots[nSlotNum] != NULL) {
                // NOTE: Leftovers of Baldur's Gate II's joinable NPCs.
                CString sScriptName(pSprite->m_scriptName);
                if (sScriptName.CompareNoCase("MINSC") == 0 && pSlots[nSlotNum]->cResRef == "MISC84") {
                    errorCode = 10218;
                    bResult = FALSE;
                } else if (sScriptName.CompareNoCase("ALORA") == 0 && pSlots[nSlotNum]->cResRef == "MISC88") {
                    errorCode = 10219;
                    bResult = FALSE;
                } else if (sScriptName.CompareNoCase("EDWIN") == 0 && pSlots[nSlotNum]->cResRef == "MISC89") {
                    errorCode = 10222;
                    bResult = FALSE;
                }
            }

            if (!bResult) {
                bDone = TRUE;
            } else {
                CItem* pOldItem = pSlots[nSlotNum];
                if (pOldItem != NULL) {
                    DWORD dwFlags = pOldItem->GetFlagsFile();
                    if ((dwFlags & 0x10) != 0) {
                        // Cursed.
                        bResult = FALSE;
                        pOldItem->m_flags |= 0x1;
                        errorCode = 16304;
                    }

                    if ((dwFlags & 0x4) == 0) {
                        // Not movable.
                        errorCode = 25697;
                        pOldItem->m_flags |= 0x1;
                        bResult = FALSE;
                        bDone = TRUE;
                    }
                }

                if (!bDone && !bResult) {
                    bDone = TRUE;
                }
            }
        }
    }

    if (!bDone && bQuickItemSlot && pItem != NULL) {
        pItem->Demand();
        ITEM_ABILITY* pAbility = pItem->GetAbility(0);
        if (pAbility == NULL) {
            errorCode = 17317;
            bResult = FALSE;
        } else if ((pItem->m_flags & 0x1) == 0) {
            if (((pAbility->type >> 8) & 0x1) != 0) {
                errorCode = 17316;
                bResult = FALSE;
            }
        } else {
            if (((pAbility->type >> 8) & 0x2) != 0) {
                errorCode = 17316;
                bResult = FALSE;
            }
        }
        pItem->Release();

        if (!bResult) {
            bDone = TRUE;
        }
    }

    if (bDone || !bResult) {
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return bResult;
    }

    if (pItem != NULL && (pItem->GetFlagsFile() & 0x10) != 0) {
        // A cursed item reveals itself and is equipped.
        pItem->m_flags |= 0x1;
        errorCode = 16304;
        bEquip = TRUE;
    }

    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        errorCode = 9373;
        return FALSE;
    }

    // NOTE: A client sends the swap to the host here (CBaldurMessage
    // 0x431560); multiplayer is removed (ADR 0002).

    CItem** pSlots = pSprite->m_equipment.m_items;
    CItem* pOldItem = pSlots[nSlotNum];

    BOOL bStacked = FALSE;
    if (pOldItem != NULL) {
        if (pItem == NULL
            && pOldItem->GetMaxStackable() > 1
            && wCount < pOldItem->GetUsageCount(0)) {
            // Take part of the stack.

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 13383
            UTIL_ASSERT(wCount != 0);

            pOldItem->SetUsageCount(0, pOldItem->GetUsageCount(0) - wCount);

            pItem = new CItem(pOldItem->cResRef, 0, 0, 0, 0, 0);
            pItem->SetUsageCount(0, wCount);
            pItem->m_flags = (pItem->m_flags & ~0x1) | (pOldItem->m_flags & 0x1);

            bStacked = TRUE;
        } else if (pItem != NULL
            && pOldItem->GetMaxStackable() > 1
            && pItem->cResRef == pOldItem->cResRef
            && ((pItem->m_flags ^ pOldItem->m_flags) & 0x1) == 0) {
            // Add to the stack.

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 13393
            UTIL_ASSERT(wCount != 0);

            INT nSpace;
            if (pItem->GetUsageCount(0) < pOldItem->GetMaxStackable() - pOldItem->GetUsageCount(0)) {
                nSpace = pItem->GetUsageCount(0);
            } else {
                nSpace = pOldItem->GetMaxStackable() - pOldItem->GetUsageCount(0);
            }

            if (wCount >= nSpace) {
                if (pItem->GetUsageCount(0) < pOldItem->GetMaxStackable() - pOldItem->GetUsageCount(0)) {
                    wCount = pItem->GetUsageCount(0);
                } else {
                    wCount = pOldItem->GetMaxStackable() - pOldItem->GetUsageCount(0);
                }
            }

            pOldItem->SetUsageCount(0, pOldItem->GetUsageCount(0) + wCount);

            if (pItem->GetUsageCount(0) == wCount) {
                // NOTE: Uninline.
                AddDisposableItem(pItem);
                pItem = NULL;
            } else {
                pItem->SetUsageCount(0, pItem->GetUsageCount(0) - wCount);
            }

            bStacked = TRUE;
        }
    }

    if (!bStacked) {
        CGameSpriteEquipment& equipment = pSprite->m_equipment;

        if (equipment.m_selectedWeapon == nSlotNum) {
            equipment.m_selectedWeapon = CGameSpriteEquipment::SLOT_FIST;
            equipment.m_selectedWeaponAbility = 0;

            if (!bEquip) {
                if (pSlots[nSlotNum] != NULL) {
                    if (nSlotNum >= 43 && nSlotNum <= 50) {
                        BYTE buttonNum = static_cast<BYTE>(nSlotNum - 43);

                        // NOTE: Uninline.
                        pSprite->SetQuickWeapon(buttonNum, CButtonData());
                        pSprite->SetQuickWeapon(buttonNum, static_cast<BYTE>(0));
                    }

                    pSlots[nSlotNum]->Demand();
                    pSlots[nSlotNum]->Unequip(pSprite, nSlotNum, TRUE, FALSE);

                    ITEM_ABILITY* pAbility = pSlots[nSlotNum]->GetAbility(equipment.m_selectedWeaponAbility);
                    if (pAbility != NULL) {
                        SHORT nLauncherSlot;
                        CItem* pLauncher = pSprite->GetLauncher(pAbility, nLauncherSlot);
                        if (pLauncher != NULL) {
                            pLauncher->Unequip(pSprite, nLauncherSlot, TRUE, FALSE);
                        }
                    }

                    pSlots[nSlotNum]->Release();
                }
            }
        } else {
            INT nSelected = equipment.m_selectedWeapon;
            pSlots[nSelected]->Demand();

            ITEM_ABILITY* pAbility = pSlots[equipment.m_selectedWeapon]->GetAbility(equipment.m_selectedWeaponAbility);
            if (pAbility != NULL) {
                SHORT nLauncherSlot;
                CItem* pLauncher = pSprite->GetLauncher(pAbility, nLauncherSlot);
                if (pLauncher != NULL && pLauncher == pSlots[nSlotNum]) {
                    // The launcher of the ammunition in use goes.
                    pSlots[nSlotNum]->Unequip(pSprite, nSlotNum, TRUE, FALSE);
                    pSlots[nSlotNum] = NULL;

                    pSlots[equipment.m_selectedWeapon]->Unequip(pSprite, equipment.m_selectedWeapon, TRUE, FALSE);
                    equipment.m_selectedWeapon = CGameSpriteEquipment::SLOT_FIST;
                    equipment.m_selectedWeaponAbility = 0;
                }
            }

            pSlots[nSelected]->Release();
        }

        if (bEquip) {
            if (pSlots[nSlotNum] != NULL) {
                pSlots[nSlotNum]->Unequip(pSprite, nSlotNum, TRUE, FALSE);
                pSlots[nSlotNum] = NULL;
            }

            if (pItem != NULL) {
                pItem->Equip(pSprite, nSlotNum, FALSE);
            }
        }

        pSlots[nSlotNum] = pItem;
        pItem = pOldItem;

        if (nSlotNum >= 43 && nSlotNum <= 50) {
            BOOL bSetWeapons = TRUE;
            if (bEquip != TRUE && pSprite->m_nWeaponSet != (nSlotNum - 43) / 2) {
                pSprite->sub_726F20();
                bSetWeapons = FALSE;
            }

            if (bSetWeapons) {
                BOOL bOffHand = nSlotNum == 44 || nSlotNum == 46 || nSlotNum == 48 || nSlotNum == 50;
                if (pOldItem != NULL && !pSprite->sub_737910(bOffHand)) {
                    bResult = FALSE;
                } else {
                    bResult = TRUE;
                    pSprite->sub_726810(static_cast<BYTE>((nSlotNum - 43) / 2));
                }
            }
        }
    }

    g_pBaldurChitin->GetActiveEngine()->UpdatePersonalItemStatus(pSprite->GetId());

    if ((g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
            || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID)
        && g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
        CMessageSpriteEquipment* pMessage = new CMessageSpriteEquipment(pSprite,
            nCharacterId,
            nCharacterId);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    // TODO: The original calls CGameSprite 0x723830 here (it sets
    // m_baseStats byte 0x49 from CRuleTables::sub_546B60, at most 5), which
    // is not in CGameSprite.cpp yet.

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return bResult;
}

// 0x5BE840
SHORT CInfGame::FindEmptyItemPersonalInventory(SHORT nPortrait)
{
    // NOTE: Uninline.
    LONG nCharacterId = GetCharacterId(nPortrait);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        for (INT nIndex = 0; nIndex < 24; nIndex++) {
            if (pSprite->m_equipment.m_items[18 + nIndex] == NULL) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return static_cast<SHORT>(nIndex + 18);
            }
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    return -1;
}

// 0x5B9BA0
BOOL CInfGame::CheckItemForClassAndKitUsability(DWORD nClassMask, DWORD nSpecialization, CItem* pItem)
{
    SHORT nType = pItem->GetItemType();
    if (nType == 35 || nType == 11) {
        return TRUE;
    }

    DWORD nNotUsable1 = pItem->GetNotUsableBy();
    DWORD nNotUsable2 = pItem->GetNotUsableBy2();

    if ((nNotUsable1 & nClassMask) == nClassMask) {
        return (nNotUsable2 & nSpecialization) != nSpecialization;
    }

    DWORD nKit = nNotUsable2 & nSpecialization;
    if (nKit != 0
        && (~(nNotUsable1 & nClassMask) & nClassMask & 0x39B) == 0
        && ((nNotUsable1 & 0x4) != 0 || (nClassMask & 0x4) == 0 || (nKit & 0xFF8000) != 0)
        && ((nNotUsable1 & 0x20) != 0 || (nClassMask & 0x20) == 0 || (nKit & 0x38) != 0)
        && ((nNotUsable1 & 0x40) != 0 || (nClassMask & 0x40) == 0 || (nKit & 0x7) != 0)
        && ((nNotUsable1 & 0x400) != 0 || (nClassMask & 0x400) == 0 || (nKit & 0x7FC0) != 0)) {
        return FALSE;
    }

    return TRUE;
}

// 0x5B9C60
BOOL CInfGame::CheckIfPlayerInPortraitSlotCanUseItem(SHORT nPortrait, CItem* pItem, STRREF* pError, BOOL bAsynch)
{
    // NOTE: Uninline.
    LONG nCharacterId = GetCharacterId(nPortrait);

    BYTE nThread = bAsynch ? CGameObjectArray::THREAD_ASYNCH : CGameObjectArray::THREAD_1;

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = GetObjectArray()->GetShare(nCharacterId,
            nThread,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        BOOL bResult = CheckIfSpriteCanUseItem(pSprite, pItem, pError, bAsynch);

        GetObjectArray()->ReleaseShare(nCharacterId,
            nThread,
            INFINITE);

        return bResult;
    }

    *pError = 9373;

    return FALSE;
}

// 0x5B9D20
BOOL CInfGame::CheckIfSpriteCanUseItem(CGameSprite* pSprite, CItem* pItem, STRREF* pError, BOOL bAsynch)
{
    BOOL bUsable = TRUE;

    if (pItem != NULL) {
        // NOTE: The original fetches the item type here and drops it.
        pItem->GetItemType();
    }

    *pError = -1;

    if (pItem == NULL) {
        return bUsable;
    }

    DWORD nNotUsable = pItem->GetNotUsableBy();

    CAIObjectType typeAI;
    typeAI.Set(pSprite->GetAIType());

    if (!g_pBaldurChitin->GetObjectGame()->GetRuleTables().IsUsableByAlignment(nNotUsable, typeAI.m_nAlignment)) {
        bUsable = FALSE;
        *pError = 9382;
    }

    if ((nNotUsable & 0x800000) != 0 && typeAI.m_nRace == CAIObjectType::R_ELF) {
        bUsable = FALSE;
        *pError = 9382;
    }

    if ((nNotUsable & 0x1000000) != 0 && typeAI.m_nRace == CAIObjectType::R_DWARF) {
        bUsable = FALSE;
        *pError = 9382;
    }

    if ((nNotUsable & 0x2000000) != 0 && typeAI.m_nRace == CAIObjectType::R_HALF_ELF) {
        bUsable = FALSE;
        *pError = 9382;
    }

    if ((nNotUsable & 0x4000000) != 0 && typeAI.m_nRace == CAIObjectType::R_HALFLING) {
        bUsable = FALSE;
        *pError = 9382;
    }

    if ((nNotUsable & 0x8000000) != 0 && typeAI.m_nRace == CAIObjectType::R_HUMAN) {
        bUsable = FALSE;
        *pError = 9382;
    }

    if ((nNotUsable & 0x10000000) != 0 && typeAI.m_nRace == CAIObjectType::R_GNOME) {
        bUsable = FALSE;
        *pError = 9382;
    }

    if ((nNotUsable & 0x20000000) != 0 && typeAI.m_nRace == CAIObjectType::R_HALF_ORC) {
        bUsable = FALSE;
        *pError = 9382;
    }

    if (!bUsable) {
        return FALSE;
    }

    if (g_pBaldurChitin->GetObjectGame()->GetRuleTables().ShouldCheckItemRequirements(pItem)) {
        if (pSprite->GetActiveStats()->m_nLevel < pItem->GetMinLevelRequired()) {
            bUsable = FALSE;
            *pError = 9382;
        }

        if (pSprite->GetActiveStats()->m_nSTR < static_cast<SHORT>(pItem->GetMinSTRRequired())) {
            bUsable = FALSE;
            *pError = 9382;
        }

        if (pSprite->GetActiveStats()->m_nINT < static_cast<SHORT>(pItem->GetMinINTRequired())) {
            bUsable = FALSE;
            *pError = 9382;
        }

        if (pSprite->GetActiveStats()->m_nDEX < static_cast<SHORT>(pItem->GetMinDEXRequired())) {
            bUsable = FALSE;
            *pError = 9382;
        }

        if (pSprite->GetActiveStats()->m_nWIS < static_cast<SHORT>(pItem->GetMinWISRequired())) {
            bUsable = FALSE;
            *pError = 9382;
        }

        if (pSprite->GetActiveStats()->m_nCON < static_cast<SHORT>(pItem->GetMinCONRequired())) {
            bUsable = FALSE;
            *pError = 9382;
        }

        if (pSprite->GetActiveStats()->m_nCHR < static_cast<SHORT>(pItem->GetMinCHRRequired())) {
            *pError = 9382;
            return FALSE;
        }
    }

    if (!bUsable) {
        return FALSE;
    }

    BOOL bResult = CheckItemForClassAndKitUsability(pSprite->GetActiveStats()->m_classMask,
        pSprite->GetActiveStats()->m_nSpecialization,
        pItem);
    if (!bResult) {
        *pError = 9382;
    }

    return bResult;
}

// The item types that go in a weapon hand.
static BOOL IsHandWeaponType(WORD nType)
{
    return nType == 0x10
        || nType == 0x11
        || nType == 0x2C
        || nType == 0x12
        || nType == 0x13
        || nType == 0x14
        || nType == 0x45
        || nType == 0x15
        || nType == 0x16
        || nType == 0x17
        || nType == 0x18
        || nType == 0x19
        || nType == 0x1A
        || nType == 0x1B
        || nType == 0x1D
        || nType == 0x1E
        || nType == 0x39;
}

// The shield item types.
static BOOL IsShieldType(WORD nType)
{
    return nType == 0x2F
        || nType == 0x35
        || nType == 0x31
        || nType == 0x29;
}

// TRUE when an ability of the item is ranged (2) or a launcher (4); sets
// the error to `strRanged` or `strLauncher` for the first one.
static BOOL HasRangedAbility(CItem* pItem, STRREF* pError, STRREF strRanged, STRREF strLauncher)
{
    BOOL bResult = FALSE;

    pItem->Demand();
    for (INT nAbility = 0; nAbility < pItem->GetAbilityCount() && !bResult; nAbility++) {
        const ITEM_ABILITY* pAbility = pItem->GetAbility(nAbility);
        if (pAbility != NULL) {
            if ((pAbility->type & 0xFF) == 2) {
                *pError = strRanged;
                bResult = TRUE;
            } else if ((pAbility->type & 0xFF) == 4) {
                *pError = strLauncher;
                bResult = TRUE;
            }
        }
    }
    pItem->Release();

    return bResult;
}

// 0x5BA2E0
BOOL CInfGame::CanEquipInSlot(CGameSprite* pSprite, SHORT nSlot, CItem** ppItem, STRREF* pError)
{
    // Whether the item (or no item) may go in the slot: the slot's item
    // types, the hands and the quick slots' rules, the usability checks and
    // the equip immunities.
    BOOL bQuickSlot = FALSE;
    CItem* pItem = *ppItem;
    CItem** pSlots = pSprite->m_equipment.m_items;

    // NOTE: Without an item the slot number stands in for the type (as the
    // original).
    WORD nType = pItem != NULL ? pItem->GetItemType() : static_cast<WORD>(nSlot);

    *pError = -1;

    if (nSlot >= CGameSpriteEquipment::NUM_SLOT) {
        *pError = 9374;
        return FALSE;
    }

    BOOL bResult = CheckIfSpriteCanUseItem(pSprite, pItem, pError, TRUE);
    if (!bResult) {
        return FALSE;
    }

    switch (nSlot) {
    case 0:
        if (pItem != NULL && nType != 1 && nType != 0x46) {
            *pError = 9375;
            return FALSE;
        }
        break;
    case 1:
        if (pItem != NULL
            && nType != 0x3C
            && nType != 0x3D
            && nType != 0x42
            && nType != 0x3E
            && nType != 0x3F
            && nType != 0x40
            && nType != 0x41
            && nType != 0x43
            && nType != 0x44) {
            *pError = 9375;
            return FALSE;
        }
        break;
    case 2:
        if (pItem != NULL && nType != 3) {
            *pError = 9375;
            return FALSE;
        }
        break;
    case 3:
        if (pItem != NULL && nType != 4) {
            *pError = 9375;
            return FALSE;
        }
        break;
    case 4:
        if (pItem != NULL && nType != 0x20) {
            *pError = 9375;
            return FALSE;
        }
        break;
    case 5:
        if (pItem != NULL && nType != 6 && nType != 0x49) {
            *pError = 9375;
            return FALSE;
        }
        break;
    case 6:
        if (pItem != NULL && nType != 7 && nType != 0x48) {
            *pError = 9375;
            return FALSE;
        }
        break;
    case 7:
    case 8:
        if (pItem != NULL && nType != 10) {
            *pError = 9375;
            return FALSE;
        }
        break;
    case 10:
        *pError = 9375;
        return FALSE;
    case 11:
    case 12:
    case 13:
    case 14:
        if (pItem != NULL && nType != 5 && nType != 0x1F && nType != 0xE) {
            *pError = 9375;
            return FALSE;
        }
        break;
    case 15:
    case 16:
    case 17:
        // The quick item slots.
        bQuickSlot = TRUE;
        if (pItem != NULL) {
            if (nType != 0
                && nType != 8
                && nType != 0x23
                && nType != 9
                && nType != 0xB
                && nType != 0xD
                && nType != 0x47) {
                *pError = 9375;
                return FALSE;
            }

            if (pItem->cResRef == "SCRL75"
                || pItem->cResRef == "MISC3P"
                || pItem->cResRef == "MISC86") {
                *pError = 9375;
                return FALSE;
            }

            if (pItem->GetAbilityCount() == 0) {
                *pError = 9375;
                return FALSE;
            }

            if (!g_pBaldurChitin->GetObjectGame()->CheckItemUsable(pSprite, pItem)) {
                *pError = 9382;
                return FALSE;
            }
        }
        break;
    case 43:
    case 45:
    case 47:
    case 49: {
        // The weapon hands; the shield hand is the next slot.
        BOOL bLauncher = FALSE;
        if (pItem != NULL) {
            if (nType == 0xF) {
                bLauncher = TRUE;
            } else if (!IsHandWeaponType(nType)) {
                *pError = 9375;
                bResult = FALSE;
            }
        }

        if (!bLauncher && (nType == 0xF || nType == 0x1B || nType == 0x12)) {
            bLauncher = TRUE;
        }

        if (pItem != NULL) {
            CItem* pOffHand = pSlots[nSlot + 1];
            if (bLauncher) {
                if (pOffHand != NULL && !IsShieldType(pOffHand->GetItemType())) {
                    *pError = 1313;
                    bResult = FALSE;
                }
            } else if (pOffHand != NULL && !IsShieldType(pOffHand->GetItemType())) {
                // A weapon in the other hand: no ranged weapon here.
                if (!bResult || HasRangedAbility(pItem, pError, 1312, 1313)) {
                    return FALSE;
                }
            }

            if ((pItem->GetFlagsFile() & 0x2) != 0 && pSlots[nSlot + 1] != NULL) {
                // Two-handed with something in the other hand.
                if (!IsShieldType(pSlots[nSlot + 1]->GetItemType())) {
                    *pError = 28758;
                } else {
                    *pError = 9380;
                }
                return FALSE;
            }
        }
        break;
    }
    case 44:
    case 46:
    case 48:
    case 50: {
        // The shield hands; the weapon hand is the slot before.
        if (pItem != NULL) {
            if (IsHandWeaponType(nType)) {
                if ((pItem->GetFlagsFile() & 0x2) != 0) {
                    *pError = 26342;
                    return FALSE;
                }

                if (HasRangedAbility(pItem, pError, 1314, 1314)) {
                    bResult = FALSE;
                }
            } else if (!IsShieldType(nType)) {
                *pError = 9375;
                return FALSE;
            }
        }

        CItem* pMainHand = pSlots[nSlot - 1];
        if (pMainHand != NULL && pItem != NULL) {
            if ((pMainHand->GetFlagsFile() & 0x2) != 0 && pMainHand != pSlots[10]) {
                *pError = 9381;
                return FALSE;
            }

            if (!IsShieldType(nType) && bResult) {
                if (HasRangedAbility(pMainHand, pError, 1310, 1311)) {
                    bResult = FALSE;
                }
            }
        }
        break;
    }
    default:
        break;
    }

    if (!bResult) {
        return FALSE;
    }

    if (pItem != NULL) {
        // NOTE: The original picks the stats in use (`m_bAllowEffectListCall`)
        // where `GetDerivedStats` is simplified to the current ones.
        CDerivedStats* pStats = pSprite->m_bAllowEffectListCall
            ? &(pSprite->m_derivedStats)
            : &(pSprite->m_tempStats);

        STRREF strError;
        CGameEffect* pEffect;
        if (pStats->m_cImmunitiesItemEquip.OnList(pItem->cResRef, strError, pEffect)
            || pStats->m_cImmunitiesItemTypeEquip.OnList(pItem->GetItemType(), strError, pEffect)) {
            *pError = strError;
            if (pEffect != NULL) {
                delete pEffect;
            }
            return FALSE;
        }
    }

    // NOTE: The original demands a NULL item here when a quick slot is asked
    // with no item; bounded here (P4-10).
    if (bQuickSlot && pItem != NULL) {
        pItem->Demand();

        if (pItem->GetAbility(0) == NULL) {
            *pError = 17317;
            pItem->Release();
            return FALSE;
        }

        if ((pItem->m_flags & 0x1) == 0 && (pItem->GetFlagsFile() & 0x100) != 0) {
            *pError = 17316;
            pItem->Release();
            return FALSE;
        }

        if ((pItem->m_flags & 0x1) != 0 && (pItem->GetFlagsFile() & 0x200) != 0) {
            *pError = 17316;
            bResult = FALSE;
        }

        pItem->Release();
    }

    return bResult;
}

// 0x5BA230
BOOL CInfGame::CanPortraitEquipInSlot(SHORT nPortrait, SHORT nSlot, CItem** ppItem, STRREF* pError)
{
    // NOTE: Uninline.
    LONG nCharacterId = GetCharacterId(nPortrait);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = GetObjectArray()->GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        *pError = 9373;
        return FALSE;
    }

    BOOL bResult = CanEquipInSlot(pSprite, nSlot, ppItem, pError);

    GetObjectArray()->ReleaseShare(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return bResult;
}

// 0x5B7730
BOOL CInfGame::CanTakeContainerItem(LONG nContainerId, SHORT nSlotNum, CGameSprite* pLooter, SHORT nPortrait)
{
    CResRef cResIcon;
    CResRef cResItem;
    CGameContainer* pContainer;

    if (m_cObjectArray.GetDeny(nContainerId, CGameObjectArray::THREAD_ASYNCH, reinterpret_cast<CGameObject**>(&pContainer), INFINITE) != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    CItem* pItem = pContainer->GetItem(nSlotNum);
    m_cObjectArray.ReleaseDeny(nContainerId, CGameObjectArray::THREAD_ASYNCH, INFINITE);

    if (pItem == NULL) {
        return FALSE;
    }

    // Gold (MISC07) merges into party gold, so it can always be taken.
    if (pItem->GetResRef() == "MISC07") {
        return TRUE;
    }

    // Otherwise the looter needs a free backpack slot (18 .. 18 + PERSONAL_INVENTORY_SIZE).
    CItem* pScanItem;
    STRREF description;
    WORD wCount;
    SHORT nSlot = 18;
    do {
        g_pBaldurChitin->GetObjectGame()->InventoryInfoPersonal(nPortrait, nSlot, pScanItem, description, cResIcon, cResItem, wCount, TRUE);
        if (cResIcon != "") {
            nSlot++;
        }
    } while (cResIcon != "" && nSlot < CScreenInventory::PERSONAL_INVENTORY_SIZE + 18);

    return nSlot < CScreenInventory::PERSONAL_INVENTORY_SIZE + 18;
}
