#include "LpcAnalyzer.h"
#include <AnalyzerHelpers.h>
#include <format>
#include <fstream>

// LAD[3:1], bit0 always ignored
enum CycleType : U8 {
  kIoRead,
  kIoWrite,
  kMemRead,
  kMemWrite,
  kDmaRead,
  kDmaWrite,
};

enum StartCode : U8 {
  kStart = 0b0000,
  kBusMasterGrant0 = 0b0010,
  kBusMasterGrant1 = 0b0011,
  kFwRead = 0b1101,
  kFwWrite = 0b1110,
  kStop = 0b1111,
};

enum SyncCode : U8 {
  kReady = 0b0000,
  kShortWait = 0b0101,
  kLongWait = 0b0110,
  kReadyMore = 0b1001,
  kError = 0b1010,
};

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
  std::array<Channel, 4 + 2> tmp_channels;
  for (size_t i = 0; i < ui_channels_.LAD.size(); i++) {
    tmp_channels[i] = ui_channels_.LAD[i].GetChannel();
  }
  tmp_channels[4 + 0] = ui_channels_.LFRAMEn.GetChannel();
  tmp_channels[4 + 1] = ui_channels_.LCLK.GetChannel();
  if (AnalyzerHelpers::DoChannelsOverlap(&tmp_channels[0],
                                         tmp_channels.size())) {
    SetErrorText("Please select different channels for each input.");
    return false;
  }

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

std::string DescribeSTART(const Frame& frame) {
  auto start = (StartCode)frame.mData1;
  std::string desc;
  switch (start) {
  case kStart:
    desc = "Start";
    break;
  case kFwRead:
    desc = "FW Read";
    break;
  case kFwWrite:
    desc = "FW Write";
    break;
  case kStop:
    desc = "Stop";
    break;
  default:
    desc = std::format("START:{:b}", (U8)start);
    break;
  }
  return desc;
}
std::string DescribeCYCTYPE_DIR(const Frame& frame) {
  auto cyctype_dir = (CycleType)frame.mData1;
  std::string desc;
  switch (cyctype_dir) {
  case kIoRead:
    desc = "IO Read";
    break;
  case kIoWrite:
    desc = "IO Write";
    break;
  case kMemRead:
    desc = "Mem Read";
    break;
  case kMemWrite:
    desc = "Mem Write";
    break;
  case kDmaRead:
    desc = "DMA Read";
    break;
  case kDmaWrite:
    desc = "DMA Write";
    break;
  default:
    desc = std::format("CYCTYPE_DIR:{:b}", (U8)cyctype_dir);
    break;
  }
  return desc;
}
std::string DescribeSIZE(const Frame& frame) {
  auto size = (U8)frame.mData1;
  std::string desc;
  switch (size) {
  case 0:
    desc = "1B";
    break;
  case 1:
    desc = "2B";
    break;
  case 3:
    desc = "4B";
    break;
  default:
    desc = std::format("SIZE:{:b}", size);
    break;
  }
  return desc;
}
std::string DescribeTURN_AROUND(const Frame& frame) {
  if (frame.mData1 == 0xff) {
    return "TAR";
  }
  return std::format("TAR:{:b}", (U8)frame.mData1);
}
std::string DisplayBaseToSpecifier(DisplayBase display_base) {
  switch (display_base) {
  case Binary:
    return "b";
  case Decimal:
    return "d";
  case Hexadecimal:
  default:
    return "x";
  }
}
std::string DescribeADDR(const Frame& frame, DisplayBase display_base) {
  auto fmt =
      std::string("ADDR:{:") + DisplayBaseToSpecifier(display_base) + "}";
  return std::vformat(fmt, std::make_format_args((U32)frame.mData1));
}
std::string DescribeCHANNEL(const Frame& frame) {
  return std::format("CHANNEL:{:b}", (U8)frame.mData1);
}
std::string DescribeDATA(const Frame& frame, DisplayBase display_base) {
  auto fmt =
      std::string("DATA:{:") + DisplayBaseToSpecifier(display_base) + "}";
  return std::vformat(fmt, std::make_format_args((U32)frame.mData1));
}
std::string DescribeSYNC(const Frame& frame) {
  auto sync = (SyncCode)frame.mData1;
  std::string desc;
  switch (sync) {
  case kReady:
    desc = "Ready";
    break;
  case kShortWait:
    desc = "ShortWait";
    break;
  case kLongWait:
    desc = "LongWait";
    break;
  case kReadyMore:
    desc = "ReadyMore";
    break;
  case kError:
    desc = "Error";
    break;
  default:
    desc = std::format("SYNC:{:b}", (U8)sync);
    break;
  }
  return desc;
}

std::string DescribeFrame(const Frame& frame, DisplayBase display_base) {
  FieldType ft = (FieldType)frame.mType;
  std::string text;
  switch (ft) {
  case kSTART:
    text = DescribeSTART(frame);
    break;
  case kCYCTYPE_DIR:
    text = DescribeCYCTYPE_DIR(frame);
    break;
  case kSIZE:
    text = DescribeSIZE(frame);
    break;
  case kTURN_AROUND:
    text = DescribeTURN_AROUND(frame);
    break;
  case kADDR:
    text = DescribeADDR(frame, display_base);
    break;
  case kCHANNEL:
    text = DescribeCHANNEL(frame);
    break;
  case kDATA:
    text = DescribeDATA(frame, display_base);
    break;
  case kSYNC:
    text = DescribeSYNC(frame);
    break;
  }
  return text;
}

void LpcAnalyzerResults::GenerateBubbleText(U64 frame_index,
                                            Channel& channel,
                                            DisplayBase display_base) {
  ClearResultStrings();
  Frame f = GetFrame(frame_index);
  std::string text = DescribeFrame(f, display_base);
  AddResultString(text.c_str());
}

void LpcAnalyzerResults::GenerateExportFile(const char* file,
                                            DisplayBase display_base,
                                            U32 export_type_user_id) {
  std::ofstream file_stream(file, std::ios::out);

  // Attempt to merge packets of the same type with sequential addresses
  struct MergedPacket {
    CycleType cyctype{};
    U32 addr{};
    std::vector<U8> data;
  } merged_packet;

  struct LpcPacket {
    bool is_valid() const {
      return cyctype.has_value() && addr.has_value() && data.has_value();
    }
    std::optional<CycleType> cyctype;
    std::optional<U32> addr;
    std::optional<U8> data;
  };

  auto write_packet = [&display_base,
                       &file_stream](const MergedPacket& packet) {
    Frame f;
    f.mType = kCYCTYPE_DIR;
    f.mData1 = packet.cyctype;
    auto type_name = DescribeFrame(f, display_base);
    f.mType = kADDR;
    f.mData1 = packet.addr;
    auto addr = DescribeFrame(f, display_base);
    std::string data;
    bool first = true;
    for (auto d : packet.data) {
      if (!first) {
        data += ' ';
      }
      data += std::format("{:02x}", d);
      first = false;
    }
    file_stream << type_name << ' ' << addr << " : " << data << std::endl;
  };

  LpcPacket packet;
  const U64 num_frames = GetNumFrames();
  for (U64 frame_index = 0; frame_index < num_frames; frame_index++) {
    Frame f = GetFrame(frame_index);
    FieldType ft = (FieldType)f.mType;
    switch (ft) {
    case kSTART:
      packet = {};
      break;
    case kCYCTYPE_DIR:
      packet.cyctype = (CycleType)f.mData1;
      break;
    case kADDR:
      packet.addr = (U32)f.mData1;
      break;
    case kDATA:
      packet.data = (U8)f.mData1;
      break;
    }

    if (packet.is_valid()) {
      if (merged_packet.data.size() == 0) {
        merged_packet.cyctype = packet.cyctype.value();
        merged_packet.addr = packet.addr.value();
        merged_packet.data.push_back(packet.data.value());
      } else if (merged_packet.cyctype == packet.cyctype &&
                 merged_packet.addr + merged_packet.data.size() ==
                     packet.addr) {
        merged_packet.data.push_back(packet.data.value());
      } else {
        write_packet(merged_packet);
        merged_packet.cyctype = packet.cyctype.value();
        merged_packet.addr = packet.addr.value();
        merged_packet.data = {};
        merged_packet.data.push_back(packet.data.value());
      }
      packet = {};
    }

    if (UpdateExportProgressAndCheckForCancel(frame_index, num_frames)) {
      return;
    }
  }

  // catch any trailing data
  if (merged_packet.data.size()) {
    write_packet(merged_packet);
  }

  UpdateExportProgressAndCheckForCancel(num_frames, num_frames);
}

void LpcAnalyzerResults::GenerateFrameTabularText(U64 frame_index,
                                                  DisplayBase display_base) {
  ClearTabularText();
  Frame f = GetFrame(frame_index);
  std::string text = DescribeFrame(f, display_base);
  AddTabularText(text.c_str());
  // force a newline in the "terminal" view
  AddTabularText("");
}

// Never seems to be called :/
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
  auto& lframe = channels_.LFRAMEn;
  auto& lck = channels_.LCLK;
  for (size_t i = 0; i < settings_.channels_.LAD.size(); i++) {
    channels_.LAD[i] = GetAnalyzerChannelData(settings_.channels_.LAD[i]);
  }
  lframe = GetAnalyzerChannelData(settings_.channels_.LFRAMEn);
  lck = GetAnalyzerChannelData(settings_.channels_.LCLK);

  while (true) {
    ReportProgress(lck->GetSampleNumber());
    auto start = NextStart();
    if (!start.has_value()) {
      // Does this actually need to be handled?
      continue;
    }

    // Next LFRAMEn may occur at any time. If before end of current cycle,
    // it means the current cycle is being aborted.
    next_lframe_ = lframe->GetSampleOfNextEdge();

    switch (start.value()) {
    case kStart:
      ProcessTargetProtocol();
      break;
    case kStop:
      // Stop cycles have one clock of inactive LFRAMEn, but we're already
      // there.
      break;
    default:
      // TODO indicate unknown cycle
      break;
    }
    results_.AddMarker(lck->GetSampleNumber(),
                       AnalyzerResults::MarkerType::Stop,
                       settings_.channels_.LFRAMEn);
    // why doesn't this generate a packet :(
    results_.CommitPacketAndStartNewPacket();
    results_.CommitResults();
  }
}

void LpcAnalyzer::SetupResults() {
  SetAnalyzerResults(&results_);
  results_.AddChannelBubblesWillAppearOn(settings_.channels_.LFRAMEn);
}

bool LpcAnalyzer::IsAborted() {
  auto& lck = channels_.LCLK;
  return lck->GetSampleOfNextEdge() >= next_lframe_;
}

bool LpcAnalyzer::AdvanceLCKToNextEdgeIfNotAborted() {
  auto& lck = channels_.LCLK;
  if (IsAborted()) {
    return false;
  }
  lck->AdvanceToNextEdge();
  return true;
}

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

std::optional<U8> LpcAnalyzer::LADRead1() {
  auto& lck = channels_.LCLK;
  // Not sure how to assert/debug log and actually see the msg...
  // Also not sure why this triggers but things seem to work fine.
  // if (lck->GetBitState() == BIT_LOW) {
  //  AnalyzerHelpers::Assert("LADRead1() must be called with LCK in HIGH
  //  state");
  //}
  if (!AdvanceLCKToNextEdgeIfNotAborted() ||
      !AdvanceLCKToNextEdgeIfNotAborted()) {
    return {};
  }
  return SyncAndReadLAD(lck->GetSampleNumber());
}

template <typename T, NibbleEndian E, size_t N>
std::optional<T> LpcAnalyzer::LADReadNibbles() {
  T val = 0;
  for (size_t i = 0; i < N; i++) {
    std::optional<T> n_clk = LADRead1();
    if (!n_clk.has_value()) {
      return {};
    }
    T n = n_clk.value();
    // bit of a hack to get accurate starting position of fields read using this
    // wrapper
    if (i == 0) {
      data_sample_start_ = channels_.LCLK->GetSampleNumber();
    }
    if constexpr (E == kLSNFirst) {
      val |= n << (i * 4);
    } else {
      val <<= 4;
      val |= n;
    }
  }
  return val;
}

bool LpcAnalyzer::AddFrame(FieldType field,
                           U64 start,
                           U64 end,
                           U64 data1,
                           U64 data2,
                           U8 flags) {
  if (start == 0) {
    start = data_sample_start_;
  }
  if (end == 0) {
    // just fudge with the next (rising) clock edge
    // TODO extend to next falling edge?
    end = channels_.LCLK->GetSampleOfNextEdge();
  }
  // NOTE: end - start must be > 0 or Logic crashes when trying to zoom to the
  // frame
  if (start >= end) {
    return false;
  }
  Frame frame{};
  frame.mStartingSampleInclusive = start;
  frame.mEndingSampleInclusive = end;
  frame.mType = field;
  frame.mData1 = data1;
  frame.mData2 = data2;
  frame.mFlags = flags;
  results_.AddFrame(frame);
  ReportProgress(frame.mEndingSampleInclusive);
  return true;
}

template <typename T>
bool LpcAnalyzer::AddFrameSimple(FieldType field, std::optional<T> data) {
  if (!data.has_value()) {
    return false;
  }
  return AddFrame(field, 0, 0, data.value());
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

  results_.AddMarker(start_sample, AnalyzerResults::Start,
                     settings_.channels_.LFRAMEn);

  // sync LCK and LFRAMEn to first LCK falling after LFRAMEn rising
  if (lck->GetBitState() == BIT_HIGH) {
    lck->AdvanceToNextEdge();
  }
  auto first_clock = lck->GetSampleNumber();
  lframe->AdvanceToAbsPosition(first_clock);

  AddFrame(kSTART, start_sample, first_clock, start.value());

  // return START value
  return start;
}

bool LpcAnalyzer::ProcessSync() {
  auto& lck = channels_.LCLK;
  // Each clock is a sync value, driven by whichever side is busy (slave).
  // Eventually (the spec has timeouts, but we probably shouldn't rely on
  // them?) a final value is driven (Ready, ReadyMore, Error) and the slave
  // stops driving. The master drives the bus afterwards for 1 clock.
  // Technically ReadyMore is only valid for DMA.
  while (true) {
    auto sync = LADRead1();
    if (!sync.has_value()) {
      // aborted during SYNC
      return false;
    }
    AddFrame(kSYNC, lck->GetSampleNumber(), 0, sync.value());
    if (sync == kReady || sync == kReadyMore || sync == kError) {
      break;
    }
  }
  return true;
}

bool LpcAnalyzer::ProcessIoMemCycles(bool is_mem, bool is_write) {
  if (is_mem) {
    if (!AddFrameSimple(kADDR, LADReadU32MSN())) {
      return false;
    }
  } else {
    if (!AddFrameSimple(kADDR, LADReadU16MSN())) {
      return false;
    }
  }
  if (is_write) {
    if (!AddFrameSimple(kDATA, LADReadU8LSN())) {
      return false;
    }
  }

  // TODO check value?
  if (!AddFrameSimple(kTURN_AROUND, LADReadU8LSN())) {
    return false;
  }

  if (!ProcessSync()) {
    return false;
  }

  if (!is_write) {
    if (!AddFrameSimple(kDATA, LADReadU8LSN())) {
      return false;
    }
  }

  if (!AddFrameSimple(kTURN_AROUND, LADReadU8LSN())) {
    return false;
  }
  return true;
}

void LpcAnalyzer::ProcessTargetProtocol() {
  auto& lck = channels_.LCLK;

  U64 sample_start = lck->GetSampleNumber();
  U8 cyctype_data = SyncAndReadLAD(sample_start);
  // TODO is it interesting to keep/show ignored bit?
  CycleType cyctype_dir = (CycleType)(cyctype_data >> 1);
  AddFrame(kCYCTYPE_DIR, sample_start, 0, cyctype_dir);

  switch (cyctype_dir) {
  case kIoRead:
  case kIoWrite:
  case kMemRead:
  case kMemWrite:
    ProcessIoMemCycles(cyctype_dir == kMemRead || cyctype_dir == kMemWrite,
                       cyctype_dir == kIoWrite || cyctype_dir == kMemWrite);
    break;
  default:
    // TODO
    break;
  }
}