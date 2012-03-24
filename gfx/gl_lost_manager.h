#pragma once

class GfxResourceHolder {
 public:
  virtual ~GfxResourceHolder();
  virtual void GLLost() = 0;
};

void gl_lost_manager_init();
void gl_lost_manager_shutdown();

void register_gl_resource_holder(GfxResourceHolder *holder);
void unregister_gl_resource_holder(GfxResourceHolder *holder);

// Notifies all objects about the loss.
void gl_lost();
