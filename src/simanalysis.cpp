#include <jitsim/simanalysis.hpp>
#include <jitsim/circuit.hpp>

namespace JITSim {

using namespace std;

SimInfo::SimInfo(const vector<Instance> &instances)
  : pre_instances(),
    post_instances(),
    is_stateful(false),
    is_primitive(false)
{
}

SimInfo::SimInfo(bool stateful)
  : pre_instances(),
    post_instances(),
    is_stateful(stateful),
    is_primitive(true)
{
}

}
