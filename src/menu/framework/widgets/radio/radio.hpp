#pragma once

class CRadio : public CObject
{
public:
    CRadio(std::string name, int *value, const std::vector<std::string> &items, std::string tooltip = "")
    {
        SetName(name);
        this->value = value;
        this->items = items;
        SetTooltip(tooltip);
    }

    void Render() override;

    int *GetValue() const { return value; }

private:
    int *value = nullptr;
    std::vector<std::string> items;
};
