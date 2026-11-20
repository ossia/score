#pragma once

// Metronomes saved before they had controls. Written once by the build that
// had none, and by a release: not to be regenerated from the current code,
// which no longer writes this.

namespace metronome_old_saves
{
//! A Metronome as the build before the controls wrote it.
const char* const old_json = R"json({"uuid":"7a20cee3-87da-42a5-9483-975aa33a7a65","ObjectName":"Process::ProcessModel","id":1,"Metadata":{"ScriptingName":"Metronome.1","Comment":"","Color":"Base1","Label":"","Touched":false},"Duration":705600000,"Height":300.0,"StartOffset":0,"LoopDuration":705600000,"Pos":[0.0,0.0],"Size":[200.0,100.0],"Loops":false,"FoldMode":0,"Outlet":{"uuid":"a1d97535-18ac-444a-8417-0cbc1692d897","ObjectName":"Outlet","id":0,"Hidden":false,"Custom":"Audio Out","Exposed":"audio out","GainInlet":{"uuid":"9a13fb32-269a-47bf-99a9-930188c1f19c","ObjectName":"Inlet","id":10000,"Hidden":false,"Custom":"Gain","Exposed":"gain","Value":{},"Init":{},"Domain":{"Float":{"Min":0.0,"Max":1.0}}},"PanInlet":{"uuid":"9a13fb32-269a-47bf-99a9-930188c1f19c","ObjectName":"Inlet","id":10001,"Hidden":false,"Custom":"Pan","Exposed":"pan","Value":{},"Init":{},"Domain":{}},"Gain":1.0,"Pan":[1.0,1.0],"Propagate":true},"BangOutlet":{"uuid":"cff96158-cc72-46d7-99dc-b6038171375b","ObjectName":"Outlet","id":1,"Hidden":false,"Custom":"Pulse Out","Exposed":"pulse out"}})json";

//! The Metronome of a document saved by a release (the looper example of the
//! documentation).
const char* const released_json = R"json({"uuid":"7a20cee3-87da-42a5-9483-975aa33a7a65","ObjectName":"Process::ProcessModel","id":7,"Metadata":{"ScriptingName":"Metronome","Comment":"","Color":"Transparent1","Label":"","Touched":true},"Duration":10231200000,"Height":300.0,"StartOffset":0,"LoopDuration":10231200000,"Pos":[-313.9042424242425,308.502],"Size":[120.0,22.0],"Loops":false,"FoldMode":0,"Outlet":{"uuid":"a1d97535-18ac-444a-8417-0cbc1692d897","ObjectName":"Outlet","id":0,"Hidden":false,"Custom":"Audio Out","Exposed":"audio out","GainInlet":{"uuid":"9a13fb32-269a-47bf-99a9-930188c1f19c","ObjectName":"Inlet","id":10000,"Hidden":false,"Custom":"Gain","Exposed":"gain","Value":{},"Init":{},"Domain":{"Float":{"Min":0.0,"Max":1.0}}},"PanInlet":{"uuid":"9a13fb32-269a-47bf-99a9-930188c1f19c","ObjectName":"Inlet","id":10001,"Hidden":false,"Custom":"Pan","Exposed":"pan","Value":{},"Init":{},"Domain":{}},"Gain":1.0,"Pan":[1.0,1.0],"Propagate":false},"BangOutlet":{"uuid":"cff96158-cc72-46d7-99dc-b6038171375b","ObjectName":"Outlet","id":1,"Hidden":false,"Custom":"Pulse Out","Exposed":"pulse out"}})json";
}
