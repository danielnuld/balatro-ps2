// Every class of the port, declared: a header that only needs pointers to another header's types includes this.
#ifndef TYPES_H
#define TYPES_H
#include "core.h"
typedef struct Node Node;
typedef struct Moveable Moveable;
typedef struct Sprite Sprite;
typedef struct AnimatedSprite AnimatedSprite;
typedef struct Particles Particles;
typedef struct UIBox UIBox;
typedef struct UIElement UIElement;
typedef struct DynaText DynaText;
typedef struct Card Card;
typedef struct CardArea CardArea;
typedef struct Blind Blind;
typedef struct Tag Tag;
typedef struct Back Back;
typedef struct Card_Character Card_Character;
typedef struct Event Event;
typedef struct EventManager EventManager;
typedef struct Controller Controller;
typedef struct Game Game;
extern Game *G; // the global G of the Lua
#endif
