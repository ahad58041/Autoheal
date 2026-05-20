// src/common/snapshot.cpp
// String conversions for the enums in snapshot.hpp.

#include "snapshot.hpp"

namespace autoheal {

const char* to_string(AnomalyType t) {
    switch (t) {
        case AnomalyType::CPU_SPIN: return "CPU_SPIN";
        case AnomalyType::MEM_LEAK: return "MEM_LEAK";
        case AnomalyType::HUNG:     return "HUNG";
    }
    return "UNKNOWN";
}

const char* to_string(HealStage s) {
    switch (s) {
        case HealStage::OBSERVED:   return "OBSERVED";
        case HealStage::STOPPED:    return "STOPPED";
        case HealStage::TERMINATED: return "TERMINATED";
        case HealStage::KILLED:     return "KILLED";
        case HealStage::RESOLVED:   return "RESOLVED";
    }
    return "UNKNOWN";
}

}  // namespace autoheal
