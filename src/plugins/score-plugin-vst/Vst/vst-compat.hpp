#ifndef VST_COMPAT_HPP
#define VST_COMPAT_HPP
/* Compatibility header for interoperability with plug-ins written
 * against the VST specification.
 * VST is a trademark of Steinberg Media Technologies GmbH
 */

#include <cstdint>
#include <cstring>

constexpr int32_t operator"" _4c(const char* txt, std::size_t N)
{
  return (txt[0] << 24) | (txt[1] << 16) | (txt[2] << 8) | (txt[3] << 0);
}

extern "C"
{
  static const constexpr auto kVstVersion = 2400;
  static const constexpr auto kEffectMagic = "VstP"_4c;
  static const constexpr auto cMagic = "CcnK"_4c;
  static const constexpr auto fMagic = "FxCk"_4c;
  static const constexpr auto bankMagic = "FxBk"_4c;
  static const constexpr auto chunkPresetMagic = "FPCh"_4c;
  static const constexpr auto chunkBankMagic = "FBCh"_4c;

  struct HostCanDos
  {
    static const constexpr auto canDoSendVstEvents = "sendVstEvents";
    static const constexpr auto canDoSendVstMidiEvent = "sendVstMidiEvent";
    static const constexpr auto canDoSendVstTimeInfo = "sendVstTimeInfo";
    static const constexpr auto canDoReceiveVstEvents = "receiveVstEvents";
    static const constexpr auto canDoReceiveVstMidiEvent = "receiveVstMidiEvent";
    static const constexpr auto canDoReportConnectionChanges = "reportConnectionChanges";
    static const constexpr auto canDoAcceptIOChanges = "acceptIOChanges";
    static const constexpr auto canDoSizeWindow = "sizeWindow";
    static const constexpr auto canDoOffline = "offline";
    static const constexpr auto canDoOpenFileSelector = "openFileSelector";
    static const constexpr auto canDoCloseFileSelector = "closeFileSelector";
    static const constexpr auto canDoStartStopProcess = "startStopProcess";
    static const constexpr auto canDoShellCategory = "shellCategory";
    static const constexpr auto canDoSendVstMidiEventFlagIsRealtime
        = "sendVstMidiEventFlagIsRealtime";
    static const constexpr auto canDoHasCockosViewAsConfig = "hasCockosViewAsConfig";
  };

  enum VstAEffectFlags
  {
    effFlagsHasEditor = 1 << 0,
    effFlagsHasClip = 1 << 1,
    effFlagsHasVu = 1 << 2,
    effFlagsCanMono = 1 << 3,
    effFlagsCanReplacing = 1 << 4,
    effFlagsProgramChunks = 1 << 5,
    effFlagsIsSynth = 1 << 8,
    effFlagsNoSoundInStop = 1 << 9,
    effFlagsExtIsAsync = 1 << 10,
    effFlagsExtHasBuffer = 1 << 11,
    effFlagsCanDoubleReplacing = 1 << 12
  };

  enum AEffectOpcodes
  {
    effOpen,
    effClose,
    effSetProgram,
    effGetProgram,
    effSetProgramName,
    effGetProgramName,
    effGetParamLabel,
    effGetParamDisplay,
    effGetParamName,
    effGetVu,
    effSetSampleRate,
    effSetBlockSize,
    effMainsChanged,
    effEditGetRect,
    effEditOpen,
    effEditClose,
    effEditDraw,
    effEditMouse,
    effEditKey,
    effEditIdle,
    effEditTop,
    effEditSleep,
    effIdentify,
    effGetChunk,
    effSetChunk,
    effProcessEvents,
    effCanBeAutomated,
    effString2Parameter,
    effGetNumProgramCategories,
    effGetProgramNameIndexed,
    effCopyProgram,
    effConnectInput,
    effConnectOutput,
    effGetInputProperties,
    effGetOutputProperties,
    effGetPlugCategory,
    effGetCurrentPosition,
    effGetDestinationBuffer,
    effOfflineNotify,
    effOfflinePrepare,
    effOfflineRun,
    effProcessVarIo,
    effSetSpeakerArrangement,
    effSetBlockSizeAndSampleRate,
    effSetBypass,
    effGetEffectName,
    effGetErrorText,
    effGetVendorString,
    effGetProductString,
    effGetVendorVersion,
    effVendorSpecific,
    effCanDo,
    effGetTailSize,
    effIdle,
    effGetIcon,
    effSetViewPosition,
    effGetParameterProperties,
    effKeysRequired,
    effGetVstVersion,
    effEditKeyDown,
    effEditKeyUp,
    effSetEditKnobMode,
    effGetMidiProgramName,
    effGetCurrentMidiProgram,
    effGetMidiProgramCategory,
    effHasMidiProgramsChanged,
    effGetMidiKeyName,
    effBeginSetProgram,
    effEndSetProgram,
    effGetSpeakerArrangement,
    effShellGetNextPlugin,
    effStartProcess,
    effStopProcess,
    effSetTotalSampleToProcess,
    effSetPanLaw,
    effBeginLoadBank,
    effBeginLoadProgram,
    effSetProcessPrecision,
    effGetNumMidiInputChannels,
    effGetNumMidiOutputChannels
  };
  using AEffectXOpcodes = AEffectOpcodes;

  enum AudioMasterOpcodes
  {
    audioMasterAutomate,
    audioMasterVersion,
    audioMasterCurrentId,
    audioMasterIdle,
    audioMasterPinConnected,
    audioMasterWantMidi = audioMasterPinConnected + 2,
    audioMasterGetTime,
    audioMasterProcessEvents,
    audioMasterSetTime,
    audioMasterTempoAt,
    audioMasterGetNumAutomatableParameters,
    audioMasterGetParameterQuantization,
    audioMasterIOChanged,
    audioMasterNeedIdle,
    audioMasterSizeWindow,
    audioMasterGetSampleRate,
    audioMasterGetBlockSize,
    audioMasterGetInputLatency,
    audioMasterGetOutputLatency,
    audioMasterGetPreviousPlug,
    audioMasterGetNextPlug,
    audioMasterWillReplaceOrAccumulate,
    audioMasterGetCurrentProcessLevel,
    audioMasterGetAutomationState,
    audioMasterOfflineStart,
    audioMasterOfflineRead,
    audioMasterOfflineWrite,
    audioMasterOfflineGetCurrentPass,
    audioMasterOfflineGetCurrentMetaPass,
    audioMasterSetOutputSampleRate,
    audioMasterGetOutputSpeakerArrangement,
    audioMasterGetVendorString,
    audioMasterGetProductString,
    audioMasterGetVendorVersion,
    audioMasterVendorSpecific,
    audioMasterSetIcon,
    audioMasterCanDo,
    audioMasterGetLanguage,
    audioMasterOpenWindow,
    audioMasterCloseWindow,
    audioMasterGetDirectory,
    audioMasterUpdateDisplay,
    audioMasterBeginEdit,
    audioMasterEndEdit,
    audioMasterOpenFileSelector,
    audioMasterCloseFileSelector,
    audioMasterEditFile,
    audioMasterGetChunkFile,
    audioMasterGetInputSpeakerArrangement
  };
  using AudioMasterOpcodesX = AudioMasterOpcodes;

  enum VstStringConstants
  {
    kVstMaxProgNameLen = 24,
    kVstMaxParamStrLen = 8,
    kVstMaxVendorStrLen = 64,
    kVstMaxProductStrLen = 64,
    kVstMaxEffectNameLen = 32,
    kVstMaxNameLen = 64,
    kVstMaxLabelLen = 64,
    kVstMaxShortLabelLen = 8,
    kVstMaxCategLabelLen = 24,
    kVstMaxFileNameLen = 100
  };
  using Vst2StringConstants = VstStringConstants;

  enum VstEventTypes
  {
    kVstMidiType = 1,
    kVstAudioType,
    kVstVideoType,
    kVstParameterType,
    kVstTriggerType,
    kVstSysExType
  };

  enum VstMidiEventFlags
  {
    kVstMidiEventIsRealtime = 1 << 0
  };

  enum VstTimeInfoFlags
  {
    kVstTransportChanged = 1,
    kVstTransportPlaying = 1 << 1,
    kVstTransportCycleActive = 1 << 2,
    kVstTransportRecording = 1 << 3,
    kVstAutomationWriting = 1 << 6,
    kVstAutomationReading = 1 << 7,
    kVstNanosValid = 1 << 8,
    kVstPpqPosValid = 1 << 9,
    kVstTempoValid = 1 << 10,
    kVstBarsValid = 1 << 11,
    kVstCyclePosValid = 1 << 12,
    kVstTimeSigValid = 1 << 13,
    kVstSmpteValid = 1 << 14,
    kVstClockValid = 1 << 15
  };

  enum VstSmpteFrameRate
  {
    kVstSmpte24fps = 0,
    kVstSmpte25fps = 1,
    kVstSmpte2997fps = 2,
    kVstSmpte30fps = 3,
    kVstSmpte2997dfps = 4,
    kVstSmpte30dfps = 5,
    kVstSmpteFilm16mm = 6,
    kVstSmpteFilm35mm = 7,
    kVstSmpte239fps = 10,
    kVstSmpte249fps = 11,
    kVstSmpte599fps = 12,
    kVstSmpte60fps = 13
  };

  enum VstHostLanguage
  {
    kVstLangEnglish = 1,
    kVstLangGerman,
    kVstLangFrench,
    kVstLangItalian,
    kVstLangSpanish,
    kVstLangJapanese
  };

  enum VstProcessPrecision
  {
    kVstProcessPrecision32,
    kVstProcessPrecision64
  };

  enum VstParameterFlags
  {
    kVstParameterIsSwitch = 1 << 0,
    kVstParameterUsesIntegerMinMax = 1 << 1,
    kVstParameterUsesFloatStep = 1 << 2,
    kVstParameterUsesIntStep = 1 << 3,
    kVstParameterSupportsDisplayIndex = 1 << 4,
    kVstParameterSupportsDisplayCategory = 1 << 5,
    kVstParameterCanRamp = 1 << 6
  };

  enum VstPinPropertiesFlags
  {
    kVstPinIsActive = 1 << 0,
    kVstPinIsStereo = 1 << 1,
    kVstPinUseSpeaker = 1 << 2
  };

  enum VstPlugCategory
  {
    kPlugCategUnknown,
    kPlugCategEffect,
    kPlugCategSynth,
    kPlugCategAnalysis,
    kPlugCategMastering,
    kPlugCategSpacializer,
    kPlugCategRoomFx,
    kPlugSurroundFx,
    kPlugCategRestoration,
    kPlugCategOfflineProcess,
    kPlugCategShell,
    kPlugCategGenerator,
    kPlugCategMaxCount
  };

  enum VstMidiProgramNameFlags
  {
    kMidiIsOmni = 1
  };

  enum VstSpeakerType
  {
    kSpeakerUndefined = 0x7fffffff,
    kSpeakerM = 0,
    kSpeakerL,
    kSpeakerR,
    kSpeakerC,
    kSpeakerLfe,
    kSpeakerLs,
    kSpeakerRs,
    kSpeakerLc,
    kSpeakerRc,
    kSpeakerS,
    kSpeakerCs = kSpeakerS,
    kSpeakerSl,
    kSpeakerSr,
    kSpeakerTm,
    kSpeakerTfl,
    kSpeakerTfc,
    kSpeakerTfr,
    kSpeakerTrl,
    kSpeakerTrc,
    kSpeakerTrr,
    kSpeakerLfe2
  };

  enum VstUserSpeakerType
  {
    kSpeakerU32 = -32,
    kSpeakerU31,
    kSpeakerU30,
    kSpeakerU29,
    kSpeakerU28,
    kSpeakerU27,
    kSpeakerU26,
    kSpeakerU25,
    kSpeakerU24,
    kSpeakerU23,
    kSpeakerU22,
    kSpeakerU21,
    kSpeakerU20,
    kSpeakerU19,
    kSpeakerU18,
    kSpeakerU17,
    kSpeakerU16,
    kSpeakerU15,
    kSpeakerU14,
    kSpeakerU13,
    kSpeakerU12,
    kSpeakerU11,
    kSpeakerU10,
    kSpeakerU9,
    kSpeakerU8,
    kSpeakerU7,
    kSpeakerU6,
    kSpeakerU5,
    kSpeakerU4,
    kSpeakerU3,
    kSpeakerU2,
    kSpeakerU1
  };

  enum VstSpeakerArrangementType
  {
    kSpeakerArrUserDefined = -2,
    kSpeakerArrEmpty = -1,
    kSpeakerArrMono = 0,
    kSpeakerArrStereo,
    kSpeakerArrStereoSurround,
    kSpeakerArrStereoCenter,
    kSpeakerArrStereoSide,
    kSpeakerArrStereoCLfe,
    kSpeakerArr30Cine,
    kSpeakerArr30Music,
    kSpeakerArr31Cine,
    kSpeakerArr31Music,
    kSpeakerArr40Cine,
    kSpeakerArr40Music,
    kSpeakerArr41Cine,
    kSpeakerArr41Music,
    kSpeakerArr50,
    kSpeakerArr51,
    kSpeakerArr60Cine,
    kSpeakerArr60Music,
    kSpeakerArr61Cine,
    kSpeakerArr61Music,
    kSpeakerArr70Cine,
    kSpeakerArr70Music,
    kSpeakerArr71Cine,
    kSpeakerArr71Music,
    kSpeakerArr80Cine,
    kSpeakerArr80Music,
    kSpeakerArr81Cine,
    kSpeakerArr81Music,
    kSpeakerArr102,
    kNumSpeakerArr
  };

  enum VstOfflineTaskFlags
  {
    kVstOfflineUnvalidParameter = 1 << 0,
    kVstOfflineNewFile = 1 << 1,
    kVstOfflinePlugError = 1 << 10,
    kVstOfflineInterleavedAudio = 1 << 11,
    kVstOfflineTempOutputFile = 1 << 12,
    kVstOfflineFloatOutputFile = 1 << 13,
    kVstOfflineRandomWrite = 1 << 14,
    kVstOfflineStretch = 1 << 15,
    kVstOfflineNoThread = 1 << 16
  };

  enum VstOfflineOption
  {
    kVstOfflineAudio,
    kVstOfflinePeaks,
    kVstOfflineParameter,
    kVstOfflineMarker,
    kVstOfflineCursor,
    kVstOfflineSelection,
    kVstOfflineQueryFiles
  };

  enum VstAudioFileFlags
  {
    kVstOfflineReadOnly = 1 << 0,
    kVstOfflineNoRateConversion = 1 << 1,
    kVstOfflineNoChannelChange = 1 << 2,
    kVstOfflineCanProcessSelection = 1 << 10,
    kVstOfflineNoCrossfade = 1 << 11,
    kVstOfflineWantRead = 1 << 12,
    kVstOfflineWantWrite = 1 << 13,
    kVstOfflineWantWriteMarker = 1 << 14,
    kVstOfflineWantMoveCursor = 1 << 15,
    kVstOfflineWantSelect = 1 << 16
  };
  enum VstVirtualKey
  {
    VKEY_BACK = 1,
    VKEY_TAB,
    VKEY_CLEAR,
    VKEY_RETURN,
    VKEY_PAUSE,
    VKEY_ESCAPE,
    VKEY_SPACE,
    VKEY_NEXT,
    VKEY_END,
    VKEY_HOME,
    VKEY_LEFT,
    VKEY_UP,
    VKEY_RIGHT,
    VKEY_DOWN,
    VKEY_PAGEUP,
    VKEY_PAGEDOWN,
    VKEY_SELECT,
    VKEY_PRINT,
    VKEY_ENTER,
    VKEY_SNAPSHOT,
    VKEY_INSERT,
    VKEY_DELETE,
    VKEY_HELP,
    VKEY_NUMPAD0,
    VKEY_NUMPAD1,
    VKEY_NUMPAD2,
    VKEY_NUMPAD3,
    VKEY_NUMPAD4,
    VKEY_NUMPAD5,
    VKEY_NUMPAD6,
    VKEY_NUMPAD7,
    VKEY_NUMPAD8,
    VKEY_NUMPAD9,
    VKEY_MULTIPLY,
    VKEY_ADD,
    VKEY_SEPARATOR,
    VKEY_SUBTRACT,
    VKEY_DECIMAL,
    VKEY_DIVIDE,
    VKEY_F1,
    VKEY_F2,
    VKEY_F3,
    VKEY_F4,
    VKEY_F5,
    VKEY_F6,
    VKEY_F7,
    VKEY_F8,
    VKEY_F9,
    VKEY_F10,
    VKEY_F11,
    VKEY_F12,
    VKEY_NUMLOCK,
    VKEY_SCROLL,
    VKEY_SHIFT,
    VKEY_CONTROL,
    VKEY_ALT,
    VKEY_EQUALS
  };

  enum VstModifierKey
  {
    MODIFIER_SHIFT = 1 << 0,
    MODIFIER_ALTERNATE = 1 << 1,
    MODIFIER_COMMAND = 1 << 2,
    MODIFIER_CONTROL = 1 << 3
  };

  enum VstFileSelectCommand
  {
    kVstFileLoad,
    kVstFileSave,
    kVstMultipleFilesLoad,
    kVstDirectorySelect
  };

  enum VstFileSelectType
  {
    kVstFileType
  };

  enum VstPanLawType
  {
    kLinearPanLaw,
    kEqualPowerPanLaw
  };

  enum VstProcessLevels
  {
    kVstProcessLevelUnknown,
    kVstProcessLevelUser,
    kVstProcessLevelRealtime,
    kVstProcessLevelPrefetch,
    kVstProcessLevelOffline
  };

  enum VstAutomationStates
  {
    kVstAutomationUnsupported,
    kVstAutomationOff,
    kVstAutomationRead,
    kVstAutomationWrite,
    kVstAutomationReadWrite
  };

  struct ERect
  {
    int16_t top;
    int16_t left;
    int16_t bottom;
    int16_t right;
  };

  struct VstEvent
  {
    int32_t type;
    int32_t byteSize;
    int32_t deltaFrames;
    int32_t flags;
    char data[16];
  };

  struct VstEvents
  {
    int32_t numEvents;
    intptr_t reserved;
    VstEvent* events[2];
  };

  struct VstMidiEvent
  {
    int32_t type;
    int32_t byteSize;
    int32_t deltaFrames;
    int32_t flags;
    int32_t noteLength;
    int32_t noteOffset;
    char midiData[4];
    char detune;
    char noteOffVelocity;
    char reserved1;
    char reserved2;
  };

  struct VstMidiSysexEvent
  {
    int32_t type;
    int32_t byteSize;
    int32_t deltaFrames;
    int32_t flags;
    int32_t dumpBytes;
    intptr_t resvd1;
    char* sysexDump;
    intptr_t resvd2;
  };

  struct VstTimeInfo
  {
    double samplePos;
    double sampleRate;
    double nanoSeconds;
    double ppqPos;
    double tempo;
    double barStartPos;
    double cycleStartPos;
    double cycleEndPos;
    int32_t timeSigNumerator;
    int32_t timeSigDenominator;
    int32_t smpteOffset;
    int32_t smpteFrameRate;
    int32_t samplesToNextClock;
    int32_t flags;
  };

  struct VstPatchChunkInfo
  {
    int32_t version;
    int32_t pluginUniqueID;
    int32_t pluginVersion;
    int32_t numElements;
    char future[48];
  };

  struct VstFileType
  {
    char name[128];
    char macType[8];
    char dosType[8];
    char unixType[8];
    char mimeType1[128];
    char mimeType2[128];
  };

  struct VstFileSelect
  {
    int32_t command;
    int32_t type;
    int32_t macCreator;
    int32_t nbFileTypes;
    VstFileType* fileTypes;
    char title[1024];
    char* initialPath;
    char* returnPath;
    int32_t sizeReturnPath;
    char** returnMultiplePaths;
    int32_t nbReturnPath;
    intptr_t reserved;
    char future[116];
  };

  struct VstVariableIo
  {
    float** inputs;
    float** outputs;
    int32_t numSamplesInput;
    int32_t numSamplesOutput;
    int32_t* numSamplesInputProcessed;
    int32_t* numSamplesOutputProcessed;
  };

  struct VstParameterProperties
  {
    float stepFloat;
    float smallStepFloat;
    float largeStepFloat;
    char label[kVstMaxLabelLen];
    int32_t flags;
    int32_t minInteger;
    int32_t maxInteger;
    int32_t stepInteger;
    int32_t largeStepInteger;
    char shortLabel[kVstMaxShortLabelLen];
    int16_t displayIndex;
    int16_t category;
    int16_t numParametersInCategory;
    int16_t reserved;
    char categoryLabel[kVstMaxCategLabelLen];
    char future[16];
  };

  struct VstPinProperties
  {
    char label[kVstMaxLabelLen];
    int32_t flags;
    int32_t arrangementType;
    char shortLabel[kVstMaxShortLabelLen];
    char future[48];
  };

  struct VstSpeakerProperties
  {
    float azimuth;
    float elevation;
    float radius;
    float reserved;
    char name[kVstMaxNameLen];
    int32_t type;
    char future[28];
  };

  struct VstSpeakerArrangement
  {
    int32_t type;
    int32_t numChannels;
    VstSpeakerProperties speakers[8];
  };

  struct MidiProgramCategory
  {
    int32_t thisCategoryIndex;
    char name[kVstMaxNameLen];
    int32_t parentCategoryIndex;
    int32_t flags;
  };

  struct MidiKeyName
  {
    int32_t thisProgramIndex;
    int32_t thisKeyNumber;
    char keyName[kVstMaxNameLen];
    int32_t reserved;
    int32_t flags;
  };

  struct MidiProgramName
  {
    int32_t thisProgramIndex;
    char name[kVstMaxNameLen];
    char midiProgram;
    char midiBankMsb;
    char midiBankLsb;
    char reserved;
    int32_t parentCategoryIndex;
    int32_t flags;
  };

  struct VstOfflineTask
  {
    char processName[96];
    double readPosition;
    double writePosition;
    int32_t readCount;
    int32_t writeCount;
    int32_t sizeInputBuffer;
    int32_t sizeOutputBuffer;
    void* inputBuffer;
    void* outputBuffer;
    double positionToProcessFrom;
    double numFramesToProcess;
    double maxFramesToWrite;
    void* extraBuffer;
    int32_t value;
    int32_t index;
    double numFramesInSourceFile;
    double sourceSampleRate;
    double destinationSampleRate;
    int32_t numSourceChannels;
    int32_t numDestinationChannels;
    int32_t sourceFormat;
    int32_t destinationFormat;
    char outputText[512];
    double progress;
    int32_t progressMode;
    char progressText[100];
    int32_t flags;
    int32_t returnValue;
    void* hostOwned;
    void* plugOwned;
    char future[1024];
  };

  struct VstAudioFile
  {
    int32_t flags;
    void* hostOwned;
    void* plugOwned;
    char name[kVstMaxFileNameLen];
    int32_t uniqueId;
    double sampleRate;
    int32_t numChannels;
    double numFrames;
    int32_t format;
    double editCursorPosition;
    double selectionStart;
    double selectionSize;
    int32_t selectedChannelsMask;
    int32_t numMarkers;
    int32_t timeRulerUnit;
    double timeRulerOffset;
    double tempo;
    int32_t timeSigNumerator;
    int32_t timeSigDenominator;
    int32_t ticksPerBlackNote;
    int32_t smpteFrameRate;
    char future[64];
  };

  struct VstAudioFileMarker
  {
    double position;
    char name[32];
    int32_t type;
    int32_t id;
    int32_t reserved;
  };

  struct VstWindow
  {
    char title[128];
    int16_t xPos;
    int16_t yPos;
    int16_t width;
    int16_t height;
    int32_t style;
    void* parent;
    void* userHandle;
    void* winHandle;
    char future[104];
  };

  struct VstKeyCode
  {
    int32_t character;
    unsigned char virt;
    unsigned char modifier;
  };

  struct fxProgram
  {
    int32_t chunkMagic;
    int32_t byteSize;
    int32_t fxMagic;
    int32_t version;
    int32_t fxID;
    int32_t fxVersion;
    int32_t numParams;
    char prgName[28];

    union
    {
      float params[1];
      struct
      {
        int32_t size;
        char chunk[1];
      } data;
    } content;
  };

  struct fxBank
  {
    int32_t chunkMagic;
    int32_t byteSize;
    int32_t fxMagic;
    int32_t version;
    int32_t fxID;
    int32_t fxVersion;
    int32_t numPrograms;
    int32_t currentProgram;
    char future[124];

    union
    {
      fxProgram programs[1];
      struct
      {
        int32_t size;
        char chunk[1];
      } data;
    } content;
  };

  struct AEffect;

  using audioMasterCallback = intptr_t (*)(
      AEffect* effect,
      int32_t opcode,
      int32_t index,
      intptr_t value,
      void* ptr,
      float opt);
  using AEffectDispatcherProc = intptr_t (*)(
      AEffect* effect,
      int32_t opcode,
      int32_t index,
      intptr_t value,
      void* ptr,
      float opt);
  using AEffectProcessProc
      = void (*)(AEffect* effect, float** inputs, float** outputs, int32_t sampleFrames);
  using AEffectProcessDoubleProc
      = void (*)(AEffect* effect, double** inputs, double** outputs, int32_t sampleFrames);
  using AEffectSetParameterProc = void (*)(AEffect* effect, int32_t index, float parameter);
  using AEffectGetParameterProc = float (*)(AEffect* effect, int32_t index);

  struct AEffect
  {
    int32_t magic;
    AEffectDispatcherProc dispatcher;
    AEffectProcessProc process;
    AEffectSetParameterProc setParameter;
    AEffectGetParameterProc getParameter;
    int32_t numPrograms;
    int32_t numParams;
    int32_t numInputs;
    int32_t numOutputs;
    int32_t flags;
    intptr_t resvd1;
    intptr_t resvd2;
    int32_t initialDelay;
    int32_t realQualities;
    int32_t offQualities;
    float ioRatio;
    void* object;
    void* user;
    int32_t uniqueID;
    int32_t version;
    AEffectProcessProc processReplacing;
    AEffectProcessDoubleProc processDoubleReplacing;
  };
}
#endif
