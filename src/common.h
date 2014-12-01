// Copyright (c) 2011-2012, Motorola Mobility, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of the Motorola Mobility, Inc. nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef WEBCL_COMMON_H_
#define WEBCL_COMMON_H_

// Node includes
#include <node.h>
#include "nan.h"
#include <string>
#ifdef LOGGING
#include <iostream>
#endif

using namespace std;

// OpenCL includes
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS

#if defined (__APPLE__) || defined(MACOSX)
  #ifdef __ECLIPSE__
    #include <gltypes.h>
    #include <gl3.h>
    #include <cl_platform.h>
    #include <cl.h>
    #include <cl_gl.h>
    #include <cl_gl_ext.h>
    #include <cl_ext.h>
  #else
    #include <OpenGL/gl3.h>
    #include <OpenGL/gl3ext.h>
    #include <OpenGL/OpenGL.h>
    #include <OpenCL/opencl.h>
    #define CL_GL_CONTEXT_KHR 0x2008
    #define CL_EGL_DISPLAY_KHR 0x2009
    #define CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR CL_INVALID_GL_CONTEXT_APPLE
  #endif
  #define HAS_clGetContextInfo
#elif defined(_WIN32)
    #include <GL/gl.h>
    #include <CL/opencl.h>
    #define strcasecmp _stricmp
#else
    #include <GL/gl.h>
    #include <GL/glx.h>
    #include <CL/opencl.h>
#endif

#ifndef CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR
  #define CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR 0x2006 
#endif
#ifndef CL_DEVICES_FOR_GL_CONTEXT_KHR
  #define CL_DEVICES_FOR_GL_CONTEXT_KHR 0x2007 
#endif

namespace {
#define JS_STR(...) v8::String::New(__VA_ARGS__)
#define JS_INT(val) v8::Integer::New(val)
#define JS_NUM(val) v8::Number::New(val)
#define JS_BOOL(val) v8::Boolean::New(val)
#define JS_RETHROW(tc) v8::Local<v8::Value>::New(tc.Exception());

#define REQ_ARGS(N)                                                     \
  if (args.Length() < (N))                                              \
    NanThrowTypeError("Expected " #N " arguments");

#define REQ_STR_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsString())                     \
    NanThrowTypeError("Argument " #I " must be a string");              \
  String::Utf8Value VAR(args[I]->ToString());

#define REQ_EXT_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsExternal())                   \
    NanThrowTypeError("Argument " #I " invalid");                       \
  Local<External> VAR = Local<External>::Cast(args[I]);

#define REQ_FUN_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsFunction())                   \
    NanThrowTypeError("Argument " #I " must be a function");            \
  Local<Function> VAR = Local<Function>::Cast(args[I]);

#define REQ_ERROR_THROW_NONE(error) if (ret == CL_##error) ThrowException(NanObjectWrapHandle(WebCLException::New(#error, ErrorDesc(CL_##error), CL_##error))); return;

#define REQ_ERROR_THROW(error) if (ret == CL_##error) return ThrowException(NanObjectWrapHandle(WebCLException::New(#error, ErrorDesc(CL_##error), CL_##error)));

#define DESTROY_WEBCL_OBJECT(obj)	\
  obj->Destructor();			\
  unregisterCLObj(obj);
  
} // namespace

namespace webcl {

const char* ErrorDesc(cl_int err);

// generic baton for async callbacks
struct Baton {
    NanCallback *callback;
    int error;
    char *error_msg;
    uint8_t *priv_info;

    // Custom user data
    v8::Persistent<v8::Value> data;

    // parent of this callback (WebCLEvent object)
    v8::Persistent<v8::Object> parent;

    Baton() : callback(NULL), error(CL_SUCCESS), error_msg(NULL), priv_info(NULL)  {}
    ~Baton() {
      if(error_msg) delete error_msg;
      if(priv_info) delete priv_info;
    }
};

class WebCLObject;
void registerCLObj(WebCLObject* obj);
void unregisterCLObj(WebCLObject* obj);
void AtExit(void* arg);

#undef None

namespace CLObjType {
enum CLObjType {
  None=0,
  Platform,
  Device,
  Context,
  CommandQueue,
  Kernel,
  Program,
  Sampler,
  Event,
  MemoryObject,
  Exception,
};
}

WebCLObject* findCLObj(void* type);

class WebCLObject : public node::ObjectWrap {
protected:
  WebCLObject() : _type(CLObjType::None) {}
  // virtual ~WebCLObject() {
  //   // printf("Destructor WebCLObject\n");
  //   // Destructor();
  //   // unregisterCLObj(this);
  // }

#define isA(value, type) ((int)value & (int)type)==(int)type
public:
  virtual void Destructor() { 
#ifdef LOGGING
    printf("In WebCLObject::Destructor\n"); 
#endif
  }
  CLObjType::CLObjType getType() { return _type; }
  bool isPlatform() const { return isA(_type, CLObjType::Platform); }
  bool isDevice() const { return isA(_type, CLObjType::Device); }
  bool isKernel() const { return isA(_type, CLObjType::Kernel); }
  bool isCommandQueue() const { return isA(_type, CLObjType::CommandQueue); }
  bool isMemoryObject() const { return isA(_type, CLObjType::MemoryObject); }
  bool isProgram() const { return isA(_type, CLObjType::Program); }
  bool isSampler() const { return isA(_type, CLObjType::Sampler); }
  bool isEvent() const { return isA(_type, CLObjType::Event); }
  bool isContext() const { return isA(_type, CLObjType::Context); }
  virtual bool isEqual(void *clObj) { return false; }

protected:
  CLObjType::CLObjType _type;
};

} // namespace webcl

#include "exceptions.h"

#endif
