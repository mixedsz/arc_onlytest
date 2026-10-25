#pragma once

class CGrid : public CObject
{
public:
    CGrid(int columns = 2, float gap = 8.0f)
    {
        this->columns = (std::max)(1, columns);
        this->gap = gap;
    }

    void Render() override;

private:
    int columns = 2;
    float gap = 8.0f;
};
