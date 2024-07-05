/*
 ** viewport-binding.cpp
 **
 ** This file is part of mkxp.
 **
 ** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
 **
 ** mkxp is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** mkxp is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "binding-types.h"
#include "binding-util.h"
#include "disposable-binding.h"
#include "flashable-binding.h"
#include "ruby/internal/core/rtypeddata.h"
#include "ruby/internal/intern/object.h"
#include "ruby/internal/special_consts.h"
#include "scene.h"
#include "sceneelement-binding.h"
#include "sharedstate.h"
#include "viewport.h"

#if RAPI_FULL > 187
DEF_TYPE(Viewport);
#else
DEF_ALLOCFUNC(Viewport);
#endif

// oh yes. this is awful. there should be a header for this
// more fun this way :3
extern rb_data_type_t ScreenWindowType;
struct ScreenWindow {
    Scene* getScene();
};

RB_METHOD(viewportInitialize) {
    Viewport *v;

    if (argc == 0 && rgssVer >= 3) {
        GFX_LOCK;
        v = new Viewport();
    // could either be a Rect or a Window
    } else if (argc == 1 || argc == 2) {
        VALUE rectObj;
        VALUE screenWindowObj = Qnil;
        Rect *rect;
        
        rb_get_args(argc, argv, "o|o", &rectObj, &screenWindowObj RB_ARG_END);
        
        rect = getPrivateDataCheck<Rect>(rectObj, RectType);

        Scene *scene = nullptr;
        if (!NIL_P(screenWindowObj)) {
            ScreenWindow* window = getPrivateDataCheck<ScreenWindow>(screenWindowObj, ScreenWindowType);
            scene = window->getScene();
            rb_iv_set(self, "screen_window", screenWindowObj); // so it doesn't get GC'd
        }
        
        GFX_LOCK;
        v = new Viewport(rect, scene);
    } else {
        int x, y, width, height;
        VALUE screenWindowObj = Qnil;

        rb_get_args(argc, argv, "iiii|o", &x, &y, &width, &height, &screenWindowObj RB_ARG_END);

        Scene *scene = nullptr;
        if (!NIL_P(screenWindowObj)) {
            ScreenWindow* window = getPrivateDataCheck<ScreenWindow>(screenWindowObj, ScreenWindowType);
            scene = window->getScene();
            rb_iv_set(self, "screen_window", screenWindowObj); // so it doesn't get GC'd
        }
        
        GFX_LOCK;
        v = new Viewport(x, y, width, height, scene);
    }
    
    setPrivateData(self, v);
    
    /* Wrap property objects */
    v->initDynAttribs();
    
    wrapProperty(self, &v->getRect(), "rect", RectType);
    wrapProperty(self, &v->getColor(), "color", ColorType);
    wrapProperty(self, &v->getTone(), "tone", ToneType);
    
    GFX_UNLOCK;
    return self;
}

DEF_GFX_PROP_OBJ_VAL(Viewport, Rect, Rect, "rect")
DEF_GFX_PROP_OBJ_VAL(Viewport, Color, Color, "color")
DEF_GFX_PROP_OBJ_VAL(Viewport, Tone, Tone, "tone")

DEF_GFX_PROP_I(Viewport, OX)
DEF_GFX_PROP_I(Viewport, OY)


RB_METHOD(setRGBOffset)
{
	double x, y, z;
	double x2, y2, z2;
	rb_get_args(argc, argv, "ffffff", &x, &y, &z, &x2, &y2, &z2);

	Viewport *v = getPrivateData<Viewport>(self);

	v->setRGBOffsetx(Vec4(x, y, z, 0));
	v->setRGBOffsety(Vec4(x2, y2, z2, 0));

	return Qnil;
}

RB_METHOD(setCubicTime)
{
	double time;
	rb_get_args(argc, argv, "f", &time);

	Viewport *v = getPrivateData<Viewport>(self);

	v->setCubicTime(time);

	return Qnil;
}

DEF_GFX_PROP_B(Viewport, Scanned)

void viewportBindingInit() {
    VALUE klass = rb_define_class("Viewport", rb_cObject);
#if RAPI_FULL > 187
    rb_define_alloc_func(klass, classAllocate<&ViewportType>);
#else
    rb_define_alloc_func(klass, ViewportAllocate);
#endif
    
    disposableBindingInit<Viewport>(klass);
    flashableBindingInit<Viewport>(klass);
    sceneElementBindingInit<Viewport>(klass);
    
    _rb_define_method(klass, "initialize", viewportInitialize);
    
    INIT_PROP_BIND(Viewport, Rect, "rect");
    INIT_PROP_BIND(Viewport, OX, "ox");
    INIT_PROP_BIND(Viewport, OY, "oy");
    INIT_PROP_BIND(Viewport, Color, "color");
    INIT_PROP_BIND(Viewport, Tone, "tone");

    _rb_define_method(klass, "setRGBOffset", setRGBOffset);
	_rb_define_method(klass, "setCubicTime", setCubicTime);
    INIT_PROP_BIND( Viewport, Scanned, "scanned");
}
