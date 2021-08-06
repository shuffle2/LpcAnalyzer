#pragma once

#include <Analyzer.h>
#include <AnalyzerChannelData.h>
#include <AnalyzerHelpers.h>
#include <AnalyzerResults.h>
#include <AnalyzerSettings.h>
#include <array>
#include <memory>
#include <optional>

struct LpcChannels {
  LpcChannels() {
    for (auto& c : LAD) {
      c = UNDEFINED_CHANNEL;
    }
    LFRAMEn = UNDEFINED_CHANNEL;
    LCLK = UNDEFINED_CHANNEL;
  }
  std::array<Channel, 4> LAD;
  Channel LFRAMEn;
  Channel LCLK;
};

SimpleArchive& operator<<(SimpleArchive& lhs, LpcChannels& rhs) {
  for (auto& c : rhs.LAD) {
    lhs << c;
  }
  lhs << rhs.LFRAMEn;
  lhs << rhs.LCLK;
  return lhs;
}

SimpleArchive& operator>>(SimpleArchive& lhs, LpcChannels& rhs) {
  for (auto& c : rhs.LAD) {
    lhs >> c;
  }
  lhs >> rhs.LFRAMEn;
  lhs >> rhs.LCLK;
  return lhs;
}

struct LpcUiChannels {
  std::array<AnalyzerSettingInterfaceChannel, 4> LAD;
  AnalyzerSettingInterfaceChannel LFRAMEn;
  AnalyzerSettingInterfaceChannel LCLK;
};

class LpcAnalyzerSettings : public AnalyzerSettings {
 public:
  LpcAnalyzerSettings();
  // Get the settings out of the interfaces, validate them, and save them to
  // your local settings vars.
  virtual bool SetSettingsFromInterfaces() final;
  // Load your settings from the provided string
  virtual void LoadSettings(const char* settings) final;
  // Save your settings to a string and return it. (use SetSettingsString,
  // return GetSettingsString)
  virtual const char* SaveSettings() final;

  LpcChannels channels_;
  LpcUiChannels ui_channels_;
};

class LpcAnalyzerResults : public AnalyzerResults {
  virtual void GenerateBubbleText(U64 frame_index,
                                  Channel& channel,
                                  DisplayBase display_base) final;
  virtual void GenerateExportFile(const char* file,
                                  DisplayBase display_base,
                                  U32 export_type_user_id) final;
  virtual void GenerateFrameTabularText(U64 frame_index,
                                        DisplayBase display_base) final;
  virtual void GeneratePacketTabularText(U64 packet_id,
                                         DisplayBase display_base) final;
  virtual void GenerateTransactionTabularText(U64 transaction_id,
                                              DisplayBase display_base) final;
};

struct LpcAnalyzerChannels {
  std::array<AnalyzerChannelData*, 4> LAD{};
  AnalyzerChannelData* LFRAMEn{};
  AnalyzerChannelData* LCLK{};
};

// The protocol encodes the expected format of successive fields in the START
// and CYCTYPE fields. We explicitly type the frames.
enum FieldType : U8 {
  kSTART,
  kCYCTYPE_DIR,
  kSIZE,
  kTURN_AROUND,
  kADDR,
  kCHANNEL,
  kDATA,
  kSYNC,
};

enum NibbleEndian {
  kLSNFirst,
  kMSNFirst,
};

class LpcAnalyzer : public Analyzer2 {
 public:
  LpcAnalyzer();
  virtual ~LpcAnalyzer() final;
  virtual void WorkerThread() final;

  // sample_rate: if there are multiple devices attached, and one is faster than
  // the other, we can sample at the speed of the faster one; and pretend the
  // slower one is the same speed.
  virtual U32 GenerateSimulationData(
      U64 newest_sample_requested,
      U32 sample_rate,
      SimulationChannelDescriptor** simulation_channels) final {
    return 0;
  }
  // provide the sample rate required to generate good simulation data
  virtual U32 GetMinimumSampleRateHz() final { return 0; }
  virtual const char* GetAnalyzerName() const final { return name_; }
  virtual bool NeedsRerun() final { return false; }

  virtual void SetupResults() final;

  bool AddFrame(FieldType field,
                U64 start,
                U64 end = 0,
                U64 data1 = 0,
                U64 data2 = 0,
                U8 flags = 0);
  template <typename T>
  bool AddFrameSimple(FieldType field, std::optional<T> data);

  bool IsAborted();
  bool AdvanceLCKToNextEdgeIfNotAborted();

  std::optional<U8> NextStart();

  U8 SyncAndReadLAD(U64 sample_number);
  std::optional<U8> LADRead1();

  template <typename T, NibbleEndian E, size_t N>
  std::optional<T> LADReadNibbles();
  std::optional<U8> LADReadU8LSN() {
    return LADReadNibbles<U8, kLSNFirst, 2>();
  }
  std::optional<U16> LADReadU16MSN() {
    return LADReadNibbles<U16, kMSNFirst, 4>();
  }
  std::optional<U32> LADReadU32MSN() {
    return LADReadNibbles<U32, kMSNFirst, 8>();
  }

  bool ProcessSync();
  bool ProcessIoMemCycles(bool is_mem, bool is_write);
  void ProcessTargetProtocol();

  static constexpr const char* name_{"LPC"};
  LpcAnalyzerSettings settings_;
  LpcAnalyzerResults results_;
  LpcAnalyzerChannels channels_;
  U64 data_sample_start_{};
  U64 next_lframe_{};
};

extern "C" {
ANALYZER_EXPORT const char* __cdecl GetAnalyzerName() {
  return LpcAnalyzer::name_;
}
ANALYZER_EXPORT Analyzer* __cdecl CreateAnalyzer() {
  return new LpcAnalyzer();
}
ANALYZER_EXPORT void __cdecl DestroyAnalyzer(Analyzer* analyzer) {
  delete analyzer;
}
}
