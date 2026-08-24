#pragma once
#include <memory>
#include <string>
#include <zenkit/DaedalusScript.hh>
#include <zenkit/DaedalusVm.hh>

struct LoadedScript {
    std::string fileName;
    zenkit::DaedalusScript script;
    // Zmień std::unique_ptr na std::shared_ptr:
    std::shared_ptr<zenkit::DaedalusVm> vm; 
};