#include "LpcAnalyzer.h"
#include <AnalyzerHelpers.h>
#include <format>

LpcAnalyzerSettings::LpcAnalyzerSettings() {
  ClearChannels();

  for (size_t i = 0; i < ui_channels_.LAD.size(); i++) {
    auto& ui = ui_channels_.LAD[i];
    auto& c = channels_.LAD[i];
    auto name = std::format("LAD[{}]", i);
    ui.SetTitleAndTooltip(name.c_str(),
                          "Multiplexed Command, Address, and Data");
    ui.SetChannel(channels_.LAD[i]);
    AddInterface(&ui);
    AddChannel(c, name.c_str(), false);
  }

  ui_channels_.LFRAMEn.SetTitleAndTooltip(
      "LFRAMEn", "Indicates start of a new cycle, termination of broken cycle");
  ui_channels_.LFRAMEn.SetChannel(channels_.LFRAMEn);
  AddInterface(&ui_channels_.LFRAMEn);
  AddChannel(channels_.LFRAMEn, "LFRAMEn", false);

  ui_channels_.LCLK.SetTitleAndTooltip("LCLK", "Clock");
  ui_channels_.LCLK.SetChannel(channels_.LCLK);
  AddInterface(&ui_channels_.LCLK);
  AddChannel(channels_.LCLK, "LCLK", false);
}

bool LpcAnalyzerSettings::SetSettingsFromInterfaces() {
  // TODO sanity check (AnalyzerHelpers::DoChannelsOverlap etc)
  for (size_t i = 0; i < channels_.LAD.size(); i++) {
    auto& c = channels_.LAD[i];
    c = ui_channels_.LAD[i].GetChannel();
  }
  channels_.LFRAMEn = ui_channels_.LFRAMEn.GetChannel();
  channels_.LCLK = ui_channels_.LCLK.GetChannel();

  ClearChannels();
  for (size_t i = 0; i < channels_.LAD.size(); i++) {
    auto& c = channels_.LAD[i];
    auto name = std::format("LAD[{}]", i);
    AddChannel(c, name.c_str(), c != UNDEFINED_CHANNEL);
  }
  AddChannel(channels_.LFRAMEn, "LFRAME",
             channels_.LFRAMEn != UNDEFINED_CHANNEL);
  AddChannel(channels_.LCLK, "LCLK", channels_.LCLK != UNDEFINED_CHANNEL);

  return true;
}

void LpcAnalyzerSettings::LoadSettings(const char* settings) {
  SimpleArchive archive;
  archive.SetString(settings);
  archive >> channels_;

  ClearChannels();
  for (size_t i = 0; i < channels_.LAD.size(); i++) {
    auto& c = channels_.LAD[i];
    auto name = std::format("LAD[{}]", i);
    AddChannel(c, name.c_str(), c != UNDEFINED_CHANNEL);
  }
  AddChannel(channels_.LFRAMEn, "LFRAME",
             channels_.LFRAMEn != UNDEFINED_CHANNEL);
  AddChannel(channels_.LCLK, "LCLK", channels_.LCLK != UNDEFINED_CHANNEL);

  for (size_t i = 0; i < channels_.LAD.size(); i++) {
    auto& c = channels_.LAD[i];
    ui_channels_.LAD[i].SetChannel(c);
  }
  ui_channels_.LFRAMEn.SetChannel(channels_.LFRAMEn);
  ui_channels_.LCLK.SetChannel(channels_.LCLK);
}

const char* LpcAnalyzerSettings::SaveSettings() {
  SimpleArchive archive;
  archive << channels_;
  return SetReturnString(archive.GetString());
}

void LpcAnalyzerResults::GenerateBubbleText(U64 frame_index,
                                            Channel& channel,
                                            DisplayBase display_base) {}
void LpcAnalyzerResults::GenerateExportFile(const char* file,
                                            DisplayBase display_base,
                                            U32 export_type_user_id) {}
void LpcAnalyzerResults::GenerateFrameTabularText(U64 frame_index,
                                                  DisplayBase display_base) {}
void LpcAnalyzerResults::GeneratePacketTabularText(U64 packet_id,
                                                   DisplayBase display_base) {}
void LpcAnalyzerResults::GenerateTransactionTabularText(
    U64 transaction_id,
    DisplayBase display_base) {}

LpcAnalyzer::LpcAnalyzer() {
  SetAnalyzerSettings(&settings_);
}

LpcAnalyzer::~LpcAnalyzer() {
  KillThread();
}

void LpcAnalyzer::WorkerThread() {
  for (size_t i = 0; i < settings_.channels_.LAD.size(); i++) {
    channels_.LAD[i] = GetAnalyzerChannelData(settings_.channels_.LAD[i]);
  }
  channels_.LFRAMEn = GetAnalyzerChannelData(settings_.channels_.LFRAMEn);
  channels_.LCLK = GetAnalyzerChannelData(settings_.channels_.LCLK);

  while (true) {
    auto start = NextStart();

    // CheckIfThreadShouldExit();
  }
}

void LpcAnalyzer::SetupResults() {
  SetAnalyzerResults(&results_);
}

enum StartCode {
  kStart = 0b0000,
  kBusMasterGrant0 = 0b0010,
  kBusMasterGrant1 = 0b0011,
  kFwRead = 0b1101,
  kFwWrite = 0b1110,
  kStop = 0b1111,
};

U8 LpcAnalyzer::SyncAndReadLAD(U64 sample_number) {
  U8 data = 0;
  for (size_t i = 0; i < channels_.LAD.size(); i++) {
    auto& c = channels_.LAD[i];
    c->AdvanceToAbsPosition(sample_number);
    U8 b = (c->GetBitState() == BIT_HIGH) ? 1 : 0;
    data |= b << i;
  }
  return data;
}

std::optional<U8> LpcAnalyzer::NextStart() {
  auto& lframe = channels_.LFRAMEn;
  auto& lck = channels_.LCLK;

  // Sync LCK and LFRAMEn when LFRAMEn is low/falling
  if (lframe->GetBitState() == BIT_HIGH) {
    lframe->AdvanceToNextEdge();
  }
  lck->AdvanceToAbsPosition(lframe->GetSampleNumber());
  // Advance LFRAMEn to rising
  lframe->AdvanceToNextEdge();
  const auto lframe_r = lframe->GetSampleNumber();

  std::optional<U8> start;
  U64 start_sample = 0;
  // Walk falling edges of LCK before LFRAMEn rising
  // START is LAD[3:0] of clock *before* LFRAMEn rising
  while (lck->GetSampleOfNextEdge() < lframe_r) {
    lck->AdvanceToNextEdge();
    if (lck->GetBitState() == BIT_LOW) {
      start_sample = lck->GetSampleNumber();
      start = SyncAndReadLAD(start_sample);
    }
  }
  if (!start.has_value()) {
    return {};
  }

  results_.AddMarker(
      start_sample,
      (start == kStop) ? AnalyzerResults::Stop : AnalyzerResults::Start,
      settings_.channels_.LFRAMEn);

  // sync everything to first LCK falling after LFRAMEn rising
  if (lck->GetBitState() == BIT_HIGH) {
    lck->AdvanceToNextEdge();
  }
  auto first_clock = lck->GetSampleNumber();
  lframe->AdvanceToAbsPosition(first_clock);
  SyncAndReadLAD(first_clock);

  results_.CommitResults();

  return start;
}
