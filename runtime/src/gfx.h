// What engine.c calls of gfx.c without going through Lua: love.graphics' push, pop, translate, scale, rotate,
// setColor, polygon, rectangle("fill") and draw of a Text or of a Quad of an Image.
int gfx_push(lua_State *L);
int gfx_pop(lua_State *L);
void gfx_translate(float x, float y);
void gfx_scale(float sx, float sy);
void gfx_rotate(float r);
void gfx_colour(float r, float g, float b, float a);
void gfx_polygon(lua_State *L, const float *xy, int n, const char *mode); // n points
void gfx_fill_rectangle(lua_State *L, float x, float y, float w, float h);
void gfx_draw_text(lua_State *L, float x, float y, float r, float sx, float sy, float ox, float oy); // the Text on top of the stack
// love.graphics.setShader for what is drawn next: none, or a card's shader by its file's name with the uniforms
// `shadow`, `dissolve`, `time`, `burn_colour_1` and `2`, and the two numbers sent under its own name (gfx.c, "shaders")
void gfx_plain(void);
void gfx_tilt(float hovering, float x, float y, float scale); // after gfx_card_shader: its vertex half
void gfx_card_shader(const char *name, int shadow, float amount, float time, const float burn1[4], const float burn2[4], float x, float y);
void gfx_draw_quad(lua_State *L, float x, float y, float r, float sx, float sy); // the Image and the Quad on top of the stack
