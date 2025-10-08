#ifndef INPUT_MAPPER_H
#define INPUT_MAPPER_H

#include <SDL.h>
#include <string>
#include <vector>
#include <map>
#include <set>

class InputMapper {
public:
    InputMapper(const std::string& actionsFile);
    void loadMappings();
    bool isActionTriggered(const std::string& action, const SDL_Event& event) const;
    const std::vector<std::string>& getKeysForAction(const std::string& action) const;
    void updateAction(const std::string& action, const std::vector<std::string>& keys);
    void save();
    const std::map<std::string, std::vector<std::string>>& getActionMappings() const;

private:
    std::string actionsFile_;
    std::map<std::string, std::vector<std::string>> actionMappings_;
    static const std::set<std::string> allowedKeys_;
};

#endif