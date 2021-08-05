#pragma once

#include <Analyzer.h>

class LpcAnalyzer : public Analyzer2 {
 public:
  virtual void WorkerThread() final {}

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

  static constexpr const char* name_{"LPC"};
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
