#ifndef INPUT_BASE_H
#define INPUT_BASE_H

#include <memory>
#include <vector>
#include "rist_output.h"

// Base class for all input types
class InputBase {
public:
    virtual ~InputBase() = default;
    
    // Start receiving input
    virtual bool start() = 0;
    
    // Process a single iteration
    virtual void process() = 0;
    
    // Stop receiving input
    virtual void stop() = 0;
    
    // Add a RIST output destination
    virtual void add_output(std::shared_ptr<RistOutput> output) {
        m_outputs.push_back(output);
    }
    
protected:
    std::vector<std::shared_ptr<RistOutput>> m_outputs;
};

#endif // INPUT_BASE_H
