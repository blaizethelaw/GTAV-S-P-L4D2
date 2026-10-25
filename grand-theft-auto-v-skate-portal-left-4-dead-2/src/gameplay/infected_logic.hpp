#pragma once
#include <string>

namespace lsa::infected_logic {

class Director {
public:
    void Update();
    void SpawnWave();
    void SpawnSpecial(const std::string& type);

private:
    float m_intensity = 0.0f;
};

} // namespace lsa::infected_logic
