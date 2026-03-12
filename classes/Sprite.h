#pragma once
#include "Entity.h"
#include "../imgui/imgui.h"

class Sprite : public Entity
{
    // sprite contains code for a simple OpenGL sprite class that is heirarchical, and can be used to draw a sprite with a texture
    // it is not intended to be a full-featured sprite class, but rather a simple one that can be used for simple games

public:
    Sprite() : 
        _parent(nullptr),
        _location(0, 0),
        _size(0,0),
        _rotation(0), 
        _scale(1),
        _color(1, 1, 1, 1),
        _localZOrder(0),
        _highlighted(false)
        { 
            _entityType = EntitySprite;
        };
    ~Sprite() { if (_retainCount > 0) release(); }
    
    // set the texture to use for this sprite
    void setPosition(float x, float y)
    {
        _location = ImVec2(x, y);
    }
    void setPosition(const ImVec2 &point)
    {
        _location = point;
    }
    void setCenterPosition(const ImVec2 &point)
    {
        _location = ImVec2(point.x - _size.x / 2, point.y - _size.y / 2);
    }
    const ImVec2 &getPosition() { return _location; }

    void setSize(float x, float y)
    {
        _size = ImVec2(x, y);
    }
    // set the rotation of the sprite
    void setRotation(float rotation) { _rotation = rotation; }
    // set the scale of the sprite
    void setScale(float scale) { _scale = scale; }
    // set the color of the sprite
    void setColor(float r, float g, float b, float a)
    {
        _color = ImVec4(r, g, b, a);
    }
    void setColor(const ImVec4 &color) { _color = color; }
    // set my Z order
    void setLocalZOrder(int localZOrder) { _localZOrder = localZOrder; }
    // get my Z order
    int getLocalZOrder() { return _localZOrder; }
    // get rotation
    float getRotation() { return _rotation; }
    // moveTo
    void moveTo(const ImVec2 &point) { _location = point; }
    // draw the sprite
    void paintSprite()
    {
        if (_size.x > 0.0f && _size.y > 0.0f) 
        {
            ImGui::SetCursorPos(_location);
            ImVec4 highlight = _highlighted ? ImVec4(1, 1, 0, 1) : ImVec4(0, 0, 0, 0);
            ImGui::Image((void*)(intptr_t)_texture, _size, ImVec2(0, 0), ImVec2(1, 1), _color, highlight);
        }
    }
	// is the mouse over this position?
	bool isMouseOver(const ImVec2 &mousePos)
    {
        return (mousePos.x >= _location.x && mousePos.x <= _location.x + _size.x && mousePos.y >= _location.y && mousePos.y <= _location.y + _size.y);
    }

    bool LoadTextureFromFile(const char* filename);
	
    // set the highlighted state
	virtual void	setHighlighted(bool yes);

	// highlight the holder while a bit is being dragged to us
	bool	highlighted();

protected:
    // the texture to use for this sprite
    // GLuint _texture;
    // the parent of this sprite
    Sprite *_parent;
    // the position of the sprite
    ImVec2  _location;
    // the size of the sprite
    ImVec2 _size;
    // the rotation of the sprite
    float _rotation;
    // the scale of the sprite
    float _scale;
    // the color of the sprite
    ImVec4  _color;
    // the local Z order
    int _localZOrder;
    // the texture we're going to draw
    ImTextureID _texture;
    // currently highlighted
   	bool	_highlighted;
    // private platform specific texture loading
    ImTextureID _loadTextureFromMemory(const unsigned char *image_data, int image_width, int image_height);
};
