#if OTTO_UI_EGL

#include "./egl_connection.hpp"

void EGLConnection::fatal(std::string message)
{
  printf((message + "\n").c_str());
  ::exit(1);
}

void EGLConnection::init()
{
  bcm_host_init();

  initEGL();
}

void EGLConnection::initEGL()
{
  static const EGLint attribute_list[] = {EGL_RED_SIZE,     8,
                                          EGL_GREEN_SIZE,   8,
                                          EGL_BLUE_SIZE,    8,
                                          EGL_ALPHA_SIZE,   8,
                                          EGL_DEPTH_SIZE,   16,
                                          EGL_STENCIL_SIZE, 8,
                                          EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                                          EGL_NONE};

  static const EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2,
                                              EGL_NONE};

  state.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

  if (state.display == EGL_NO_DISPLAY) {
    fatal("Error opening EGL display");
  }

  EGLBoolean result = eglInitialize(state.display, nullptr, nullptr);
  if (result == EGL_FALSE) {
    fatal("Error initializing EGL display");
  }

  EGLConfig config;
  result = eglChooseConfig(state.display, attribute_list, &config, 1,
                           &eglData.nConfig);
  if (result == EGL_FALSE) {
    fatal("Error choosing EGL config");
  }

  result = eglBindAPI(EGL_OPENGL_ES_API);
  if (result == EGL_FALSE) {
    fatal("Error binding EGL API");
  }

  state.context =
    eglCreateContext(state.display, config, EGL_NO_CONTEXT, context_attributes);
  if (state.context == EGL_NO_CONTEXT) {
    fatal("Error creating EGL context");
  }

  int success = graphics_get_display_size(0, &state.width, &state.height);
  if (success < 0) {
    fatal("Error getting display size");
  }

  // What does this do?
  vc_dispmanx_rect_set(&eglData.rect, 0, 0, state.width, state.height);
  eglData.width          = state.width;
  eglData.height         = state.height;
  eglData.screenDatasize = eglData.width * eglData.height * eglData.bitdepth;
  eglData.data           = (unsigned short*) malloc(eglData.screenDatasize);

  // What does this do?
  uint imagePrt;
  eglData.resource = vc_dispmanx_resource_create(VC_IMAGE_RGB565, state.width,
                                                 state.height, &imagePrt);

  if (!eglData.resource) {
    fatal("Error getting resource");
  }

  VCRect srcRect;
  VCRect dstRect;

  dstRect.x      = 0;
  dstRect.y      = 0;
  dstRect.width  = state.width;
  dstRect.height = state.height;

  srcRect.x      = 0;
  srcRect.y      = 0;
  srcRect.width  = state.width << 16;
  srcRect.height = state.height << 16;

  eglData.display = vc_dispmanx_display_open(0);
  if (!eglData.display) {
    fatal("Error opening display");
  }

  DMXDisplay dmxDisplay = eglData.display;
  DMXUpdate dmxUpdate   = vc_dispmanx_update_start(0);
  DMXElement dmxElement = vc_dispmanx_element_add(
    dmxUpdate, dmxDisplay, 0, &dstRect, 0, &srcRect, DISPMANX_PROTECTION_NONE,
    0, 0, DISPMANX_TRANSFORM_T());

  eglData.nativeWindow.element = dmxElement;
  eglData.nativeWindow.width   = state.width;
  eglData.nativeWindow.height  = state.height;

  vc_dispmanx_update_submit_sync(dmxUpdate);

  state.surface = eglCreateWindowSurface(state.display, config,
                                         &eglData.nativeWindow, nullptr);

  if (state.surface == EGL_NO_SURFACE) {
    fatal("Error creating EGL surface");
  }

  result =
    eglMakeCurrent(state.display, state.surface, state.surface, state.context);

  if (result == EGL_FALSE) {
    fatal("Error connecting the context to the surface");
  }

  // Set background color and clear buffers
  glClearColor(1.f, 1.f, 1.f, 1.f);

  // Enable back face culling.
  glEnable(GL_CULL_FACE);
}

void EGLConnection::exit()
{
  glClear(GL_COLOR_BUFFER_BIT);
  eglSwapBuffers(state.display, state.surface);

  // Release OpenGL resources
  eglDestroySurface(state.display, state.surface);

  DMXUpdate dmxUpdate = vc_dispmanx_update_start(0);
  vc_dispmanx_element_remove(dmxUpdate, eglData.nativeWindow.element);
  vc_dispmanx_update_submit_sync(dmxUpdate);
  vc_dispmanx_resource_delete(eglData.resource);
  vc_dispmanx_display_close(eglData.display);

  eglMakeCurrent(state.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(state.display, state.context);
  eglTerminate(state.display);

  free(eglData.data);
}

void EGLConnection::beginFrame()
{
  // Start with a clear screen
  glViewport(0, 0, eglData.width, eglData.height);

  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_CULL_FACE);
  glEnable(GL_STENCIL_TEST);
}

void EGLConnection::endFrame()
{
  eglSwapBuffers(state.display, state.surface);
}

#endif // OTTO_UI_EGL
