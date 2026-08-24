#pragma once
#include <memory>
#include <string>
#include <iostream>
#include <zenkit/DaedalusScript.hh>
#include <zenkit/DaedalusVm.hh>

struct LoadedScript {
    std::string fileName;
    zenkit::DaedalusScript script;
    std::shared_ptr<zenkit::DaedalusVm> vm; 

    // Przeciążenie operatora << dla std::cout
    friend std::ostream& operator<<(std::ostream& os, const LoadedScript& ls) {
        os << "LoadedScript [File: " << ls.fileName 
           << ", VM Status: " << (ls.vm ? "Active" : "Null") << "]";
        return os;
    }
};