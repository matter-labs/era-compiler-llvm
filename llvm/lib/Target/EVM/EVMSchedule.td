def EVMModel : SchedMachineModel {
  let MicroOpBufferSize = 0;  // EVM is in-order processor
  let IssueWidth = 1;         // 4 micro-ops dispatched per cycle
  let LoadLatency = 1;        // Optimistic, assuming no misses
  let MispredictPenalty = 0;  // A branch direction mispredict
  let CompleteModel = 0;      // Covers instructions applicable to EVM.
}

def VMProc : ProcResource<1> { let BufferSize = 0; }
def WriteRes : SchedWrite;

let SchedModel = EVMModel in {
  def : WriteRes<WriteRes, [VMProc]> { let Latency = 1; }
}
