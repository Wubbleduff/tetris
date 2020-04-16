//
// References:
//
// https://handmade.network/wiki/2834-tutorial_a_tour_through_xlib_and_related_technologies
// https://www.khronos.org/opengl/wiki/Tutorial:_OpenGL_3.0_Context_Creation_(GLX)
//

#include "renderer.h"
#include "input.h"

#include <stdio.h>  // printf
#include <string.h> // string operations
#include <stdlib.h>



#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <GL/gl.h>
#include <GL/glx.h>

#include <vector>

#include <assert.h>





struct CellData
{
  v2i position;
  Color color;
};

struct Graphics
{
    bool window_open = false;
    Display *display = nullptr;
    Window window;
    Colormap color_map;
    int window_width;
    int window_height;

    XIC x_input_context;

    Atom wm_delete_window;

    GLXContext gl_context;

    GLuint vao;
    GLuint ebo;
    GLuint shader_program;




    bool keys_down[256];
    bool prev_keys_down[256];

    std::vector<CellData> cells_to_render;
    std::vector<CellData> left_bar_cells_to_render;
    std::vector<CellData> right_bar_cells_to_render;
};

typedef void (APIENTRYP PFNGLGENVERTEXARRAYS) (GLuint, GLuint *);
typedef void (APIENTRYP PFNGLBINDVERTEXARRAY) (GLuint);
typedef void (APIENTRYP PFNGLVERTEXATTRIBPOINTER) (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
typedef void (APIENTRYP PFNGLGLENABLEVERTEXATTRIBARRAY) (GLuint);

typedef GLuint (APIENTRYP PFNGLCREATESHADER) (GLenum);
typedef void (APIENTRYP PFNGLSHADERSOURCE) (GLuint, GLsizei, const GLchar **, const GLint *);
typedef void (APIENTRYP PFNGLCOMPILESHADER) (GLuint);
typedef GLuint (APIENTRYP PFNGLCREATEPROGRAM) (void);
typedef void (APIENTRYP PFNGLATTACHSHADER) (GLuint, GLuint);
typedef void (APIENTRYP PFNGLLINKPROGRAM) (GLuint);
typedef void (APIENTRYP PFNGLDELETESHADER) (GLuint);
typedef void (APIENTRYP PFNGLUSEPROGRAM) (GLuint);
typedef void (APIENTRYP PFNGLGETSHADERIV) (GLuint, GLenum, GLint *);
typedef void (APIENTRYP PFNGLGETSHADERINFOLOG) (GLuint, GLsizei, GLsizei *, GLchar *);
typedef void (APIENTRYP PFNGLGETPROGRAMIV) (GLuint, GLenum, GLint *);
typedef void (APIENTRYP PFNGLGETPROGRAMINFOLOG) (GLuint, GLsizei, GLsizei *, GLchar *);
typedef GLint (APIENTRYP PFNGLGETUNIFORMLOCATION) (GLuint, const GLchar *);
typedef void (APIENTRYP PFNGLUNIFORM1FV) (GLint, GLsizei, const GLfloat *);
typedef void (APIENTRYP PFNGLUNIFORM3F) (GLint, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRYP PFNGLUNIFORM2F) (GLint, GLfloat, GLfloat);

typedef void (APIENTRYP PFNGLGENBUFFERS) (GLuint, GLuint *);
typedef void (APIENTRYP PFNGLBINDBUFFER) (GLenum, GLuint);
typedef void (APIENTRYP PFNGLBUFFERDATA) (GLenum, GLsizeiptr, const void *, GLenum);
struct GLFunctions
{
    PFNGLGENVERTEXARRAYS glGenVertexArrays;
    PFNGLBINDVERTEXARRAY glBindVertexArray;
    PFNGLVERTEXATTRIBPOINTER glVertexAttribPointer;
    PFNGLGLENABLEVERTEXATTRIBARRAY glEnableVertexAttribArray;
    PFNGLCREATESHADER glCreateShader;
    PFNGLSHADERSOURCE glShaderSource;
    PFNGLCOMPILESHADER glCompileShader;
    PFNGLCREATEPROGRAM glCreateProgram;
    PFNGLATTACHSHADER glAttachShader;
    PFNGLLINKPROGRAM glLinkProgram;
    PFNGLDELETESHADER glDeleteShader;
    PFNGLUSEPROGRAM glUseProgram;
    PFNGLGETSHADERIV glGetShaderiv;
    PFNGLGETSHADERINFOLOG glGetShaderInfoLog;
    PFNGLGETPROGRAMIV glGetProgramiv;
    PFNGLGETPROGRAMINFOLOG glGetProgramInfoLog;
    PFNGLGENVERTEXARRAYS glGenBuffers;
    PFNGLBINDBUFFER glBindBuffer;
    PFNGLBUFFERDATA glBufferData;
    PFNGLGETUNIFORMLOCATION glGetUniformLocation;
    PFNGLUNIFORM1FV glUniform1fv;
    PFNGLUNIFORM3F glUniform3f;
    PFNGLUNIFORM2F glUniform2f;
};

static Graphics *graphics;
static GLFunctions *gl_fn;

static const int GRID_WIDTH = 10.0f;
static const int GRID_HEIGHT = 24.0f;
static const float GRID_ASPECT_RATIO = (float)GRID_WIDTH / (float)GRID_HEIGHT;









#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

#define GL_VERSION_MAJOR 3
#define GL_VERSION_MINOR 3

void check_gl_errors(const char *tag)
{
    int error = glGetError();
    if(error) printf("%s: GL Error %d\n", tag, error);
}

// Helper to check for extension string presence.  Adapted from:
// //   http://www.opengl.org/resources/features/OGLextensions/
static bool isExtensionSupported(const char *extList, const char *extension)
{
    const char *start;
    const char *where, *terminator;

    /* Extension names should not have spaces. */
    where = strchr(extension, ' ');
    if (where || *extension == '\0')
        return false;

    /* It takes a bit of care to be fool-proof about parsing the
       OpenGL extensions string. Don't be fooled by sub-strings,
       etc. */
    for (start=extList;;) {
        where = strstr(start, extension);

        if (!where)
            break;

        terminator = where + strlen(extension);

        if ( where == start || *(where - 1) == ' ' )
            if ( *terminator == ' ' || *terminator == '\0' )
                return true;

        start = terminator;
    }

    return false;
}

static bool ctxErrorOccurred = false;
static int ctxErrorHandler( Display *dpy, XErrorEvent *ev )
{
    ctxErrorOccurred = true;
    return 0;
}



void set_size_hint(Display *display, Window window,
                  int min_width, int min_height,
                  int max_width, int max_height)
{
    XSizeHints hints = {};
    if(min_width > 0 && min_height > 0) hints.flags |= PMinSize;
    if(max_width > 0 && max_height > 0) hints.flags |= PMaxSize;

    hints.min_width  = min_width;
    hints.min_height = min_height;
    hints.max_width  = max_width;
    hints.max_height = max_height;

    XSetWMNormalHints(display, window, &hints);
}

Status toggle_maximize(Display *display, Window window) 
{  
    XClientMessageEvent ev = {};
    Atom wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom max_h = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    Atom max_v = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);

    if(wm_state == None) return 0;

    ev.type = ClientMessage;
    ev.format = 32;
    ev.window = window;
    ev.message_type = wm_state;
    ev.data.l[0] = 2; // _NET_WM_STATE_TOGGLE 2 according to spec; Not defined in my headers
    ev.data.l[1] = max_h;
    ev.data.l[2] = max_v;
    ev.data.l[3] = 1;

    return XSendEvent(display, DefaultRootWindow(display), False,
            SubstructureNotifyMask,
            (XEvent *)&ev);
}

static void load_gl_fn()
{
    gl_fn->glGenVertexArrays = (PFNGLGENVERTEXARRAYS)glXGetProcAddress((const GLubyte *)"glGenVertexArrays");
    gl_fn->glBindVertexArray = (PFNGLBINDVERTEXARRAY)glXGetProcAddress((const GLubyte *)"glBindVertexArray");
    gl_fn->glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTER)glXGetProcAddress((const GLubyte *)"glVertexAttribPointer");
    gl_fn->glEnableVertexAttribArray = (PFNGLGLENABLEVERTEXATTRIBARRAY)glXGetProcAddress((const GLubyte *)"glEnableVertexAttribArray");
    gl_fn->glCreateShader = (PFNGLCREATESHADER)glXGetProcAddress((const GLubyte *)"glCreateShader");
    gl_fn->glShaderSource = (PFNGLSHADERSOURCE)glXGetProcAddress((const GLubyte *)"glShaderSource");
    gl_fn->glCompileShader = (PFNGLCOMPILESHADER)glXGetProcAddress((const GLubyte *)"glCompileShader");
    gl_fn->glCreateProgram = (PFNGLCREATEPROGRAM)glXGetProcAddress((const GLubyte *)"glCreateProgram");
    gl_fn->glAttachShader = (PFNGLATTACHSHADER)glXGetProcAddress((const GLubyte *)"glAttachShader");
    gl_fn->glLinkProgram = (PFNGLLINKPROGRAM)glXGetProcAddress((const GLubyte *)"glLinkProgram");
    gl_fn->glDeleteShader = (PFNGLDELETESHADER)glXGetProcAddress((const GLubyte *)"glDeleteShader");
    gl_fn->glUseProgram = (PFNGLUSEPROGRAM)glXGetProcAddress((const GLubyte *)"glUseProgram");
    gl_fn->glGetShaderiv = (PFNGLGETSHADERIV)glXGetProcAddress((const GLubyte *)"glGetShaderiv");
    gl_fn->glGetShaderInfoLog = (PFNGLGETSHADERINFOLOG)glXGetProcAddress((const GLubyte *)"glGetShaderInfoLog");
    gl_fn->glGetProgramiv = (PFNGLGETPROGRAMIV)glXGetProcAddress((const GLubyte *)"glGetProgramiv");
    gl_fn->glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOG)glXGetProcAddress((const GLubyte *)"glGetProgramInfoLog");
    gl_fn->glGenBuffers = (PFNGLGENBUFFERS)glXGetProcAddress((const GLubyte *)"glGenBuffers");
    gl_fn->glBindBuffer = (PFNGLBINDBUFFER)glXGetProcAddress((const GLubyte *)"glBindBuffer");
    gl_fn->glBufferData = (PFNGLBUFFERDATA)glXGetProcAddress((const GLubyte *)"glBufferData");
    gl_fn->glGetUniformLocation = (PFNGLGETUNIFORMLOCATION)glXGetProcAddress((const GLubyte *)"glGetUniformLocation");;
    gl_fn->glUniform1fv = (PFNGLUNIFORM1FV)glXGetProcAddress((const GLubyte *)"glUniform1fv");;
    gl_fn->glUniform3f = (PFNGLUNIFORM3F)glXGetProcAddress((const GLubyte *)"glUniform3f");;
    gl_fn->glUniform2f = (PFNGLUNIFORM2F)glXGetProcAddress((const GLubyte *)"glUniform2f");;
}

void init_graphics()
{
    graphics = (Graphics *)malloc(sizeof(Graphics));
    if(!graphics) return;
    memset(graphics, 0, sizeof(Graphics));
    gl_fn = (GLFunctions *)malloc(sizeof(GLFunctions));
    if(!gl_fn) return;
    memset(gl_fn, 0, sizeof(GLFunctions));



    graphics->display = XOpenDisplay(NULL);
    if(!graphics->display)
    {
        printf("Failed to open X display\n");
        return;
    }

    int root_window = DefaultRootWindow(graphics->display);
    int default_screen = DefaultScreen(graphics->display);

    graphics->window_width  = 100 * 3;
    graphics->window_height = 240 * 3;


    // Get a matching FB config
    static int visual_attribs[] =
    {
        GLX_X_RENDERABLE    , True,
        GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
        GLX_RENDER_TYPE     , GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
        GLX_RED_SIZE        , 8,
        GLX_GREEN_SIZE      , 8,
        GLX_BLUE_SIZE       , 8,
        GLX_ALPHA_SIZE      , 8,
        GLX_DEPTH_SIZE      , 24,
        GLX_STENCIL_SIZE    , 8,
        GLX_DOUBLEBUFFER    , True,
        //GLX_SAMPLE_BUFFERS  , 1,
        //GLX_SAMPLES         , 4,
        None
    };

    int glx_major, glx_minor;

    // FBConfigs were added in GLX version 1.3.
    if(!glXQueryVersion(graphics->display, &glx_major, &glx_minor) || 
       ((glx_major == 1) && (glx_minor < 3)) || (glx_major < 1))
    {
        printf("Invalid GLX version");
        return;
    }

    printf("Getting matching framebuffer configs\n");
    int fbcount;
    GLXFBConfig *fbc = glXChooseFBConfig(graphics->display, default_screen, visual_attribs, &fbcount);
    if(!fbc)
    {
        printf("Failed to retrieve a framebuffer config\n");
        return;
    }
    printf("Found %d matching FB configs.\n", fbcount);

    // Pick the FB config/visual with the most samples per pixel
    printf("Getting XVisualInfos\n");
    int best_fbc_index = -1, worst_fbc = -1, best_num_samp = -1, worst_num_samp = 999;

    for(int i = 0; i < fbcount; i++)
    {
        XVisualInfo *vi = glXGetVisualFromFBConfig(graphics->display, fbc[i]);
        if(vi)
        {
            int samp_buf, samples;
            glXGetFBConfigAttrib(graphics->display, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf);
            glXGetFBConfigAttrib(graphics->display, fbc[i], GLX_SAMPLES, &samples);

            // printf("  Matching fbconfig %d, visual ID 0x%2x: SAMPLE_BUFFERS = %d,"
            //     " SAMPLES = %d\n", 
            //     i, vi -> visualid, samp_buf, samples);

            if(best_fbc_index < 0 || samp_buf && samples > best_num_samp)
            {
                best_fbc_index = i;
                best_num_samp = samples;
            }
            if(worst_fbc < 0 || !samp_buf || samples < worst_num_samp)
            {
                worst_fbc = i;
                worst_num_samp = samples;
            }
        }
        XFree(vi);
    }

    GLXFBConfig best_fbc = fbc[best_fbc_index];

    // Be sure to free the FBConfig list allocated by glXChooseFBConfig()
    XFree(fbc);

    // Get a visual
    XVisualInfo *vi = glXGetVisualFromFBConfig(graphics->display, best_fbc);
    printf("Chosen visual ID = 0x%x\n", vi->visualid);

    printf("Creating colormap\n");
    XSetWindowAttributes swa;
    swa.colormap = graphics->color_map = XCreateColormap(graphics->display,
            root_window, vi->visual, AllocNone);
    swa.background_pixmap = None ;
    swa.border_pixel      = 0;
    swa.event_mask        = StructureNotifyMask | KeyPressMask | KeyReleaseMask;;

    printf("Creating window\n");
    // use this as root? " RootWindow( display, vi->screen) "
    graphics->window = XCreateWindow(graphics->display, root_window, 
            0, 0, graphics->window_width, graphics->window_height, 0, vi->depth, InputOutput, 
            vi->visual, 
            CWBorderPixel|CWColormap|CWEventMask, &swa);
    if(!graphics->window)
    {
        printf("Failed to create window.\n");
        return;
    }
    graphics->window_open = true;

    // Done with the visual info data
    XFree(vi);

    XStoreName(graphics->display, graphics->window, "GL Window");

    printf("Mapping window\n");
    XMapWindow(graphics->display, graphics->window);

    // Get the default screen's GLX extension list
    const char *glxExts = glXQueryExtensionsString(graphics->display, default_screen);

    // NOTE: It is not necessary to create or make current to a context before
    // calling glXGetProcAddressARB
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte *)"glXCreateContextAttribsARB");

    graphics->gl_context = 0;

    // Install an X error handler so the application won't exit if GL x.x
    // context allocation fails.
    //
    // Note this error handler is global.  All display connections in all threads
    // of a process use the same error handler, so be sure to guard against other
    // threads issuing X commands while this code is running.
    ctxErrorOccurred = false;
    int (*oldHandler)(Display *, XErrorEvent *) = XSetErrorHandler(&ctxErrorHandler);

    // Check for the GLX_ARB_create_context extension string and the function.
    // If either is not present, use GLX 1.3 context creation method.
    if(!isExtensionSupported(glxExts, "GLX_ARB_create_context") || !glXCreateContextAttribsARB)
    {
        printf( "glXCreateContextAttribsARB() not found ... using old-style GLX context\n" );
        graphics->gl_context = glXCreateNewContext( graphics->display, best_fbc, GLX_RGBA_TYPE, 0, True );
    }

    // If it does, try to get a GL x.x context!
    else
    {
        int context_attribs[] =
        {
            GLX_CONTEXT_MAJOR_VERSION_ARB, GL_VERSION_MAJOR,
            GLX_CONTEXT_MINOR_VERSION_ARB, GL_VERSION_MINOR,
            //GLX_CONTEXT_FLAGS_ARB        , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
            None
        };

        printf("Creating context\n");
        graphics->gl_context = glXCreateContextAttribsARB(graphics->display, best_fbc, 0, True, context_attribs);

        // Sync to ensure any errors generated are processed.
        XSync(graphics->display, False);
        if(!ctxErrorOccurred && graphics->gl_context)
        {
            printf("Created GL %d.%d context\n", GL_VERSION_MAJOR, GL_VERSION_MINOR);
        }
        else
        {
            // Couldn't create GL x.x context.  Fall back to old-style 2.x context.
            // When a context version below x.x is requested, implementations will
            // return the newest context version compatible with OpenGL versions less
            // than version x.x.
            // GLX_CONTEXT_MAJOR_VERSION_ARB = 1
            context_attribs[1] = 1;
            // GLX_CONTEXT_MINOR_VERSION_ARB = 0
            context_attribs[3] = 0;

            ctxErrorOccurred = false;

            printf("Failed to create GL %d.%d context ... using old-style GLX context\n", GL_VERSION_MAJOR< GL_VERSION_MINOR);
            graphics->gl_context = glXCreateContextAttribsARB( graphics->display, best_fbc, 0, True, context_attribs);
        }
    }




    // Set minimum size
    set_size_hint(graphics->display, graphics->window, 400, 300, 0, 0);


    // Input
    {
        XIM xInputMethod = XOpenIM(graphics->display, 0, 0, 0);
        if(!xInputMethod)
        {
            printf("Input Method could not be opened\n");
        }

        XIMStyles* styles = 0;
        if(XGetIMValues(xInputMethod, XNQueryInputStyle, &styles, NULL) || !styles)
        {
            printf("Input Styles could not be retrieved\n");
        }

        XIMStyle bestMatchStyle = 0;
        for(int i=0; i<styles->count_styles; i++)
        {
            XIMStyle thisStyle = styles->supported_styles[i];
            if (thisStyle == (XIMPreeditNothing | XIMStatusNothing))
            {
                bestMatchStyle = thisStyle;
                break;
            }
        }
        XFree(styles);

        if(!bestMatchStyle)
        {
            printf("No matching input style could be determined\n");
        }

        graphics->x_input_context = XCreateIC(xInputMethod, XNInputStyle, bestMatchStyle,
                XNClientWindow, graphics->window,
                XNFocusWindow, graphics->window,
                NULL);
        if(!graphics->x_input_context)
        {
            printf("Input Context could not be created\n");
        }
    }

    // Sync to ensure any errors generated are processed.
    XSync(graphics->display, False);

    XFlush(graphics->display); // Idk if this does the same thing as XSync


    // Restore the original error handler
    XSetErrorHandler(oldHandler);

    if(ctxErrorOccurred || !graphics->gl_context)
    {
        printf("Failed to create an OpenGL context\n");
        return;
    }

    // Verifying that context is a direct context
    if(!glXIsDirect(graphics->display, graphics->gl_context))
    {
        printf("Indirect GLX rendering context obtained\n");
    }
    else
    {
        printf("Direct GLX rendering context obtained\n");
    }

    printf("Making context current\n");
    glXMakeCurrent(graphics->display, graphics->window, graphics->gl_context);
    printf("Shading language: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));




    // Register for "delete ourselves" event to play nice with X and with WM
    graphics->wm_delete_window = XInternAtom(graphics->display, "WM_DELETE_WINDOW", False);
    if(!XSetWMProtocols(graphics->display, graphics->window, &graphics->wm_delete_window, 1))
    {
        printf("Couldn't register WM_DELETE_WINDOW property\n");
    }

    load_gl_fn();






    // Mesh
    {
        GLuint vao;
        gl_fn->glGenVertexArrays(1, &vao);
        gl_fn->glBindVertexArray(vao);
        check_gl_errors("create vao");

        float vertices[] =
        {
            0.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
        };
        int indices[] =
        {
            0, 1, 2,
            0, 2, 3
        };

        GLuint vbo;
        gl_fn->glGenBuffers(1, &vbo);
        gl_fn->glBindBuffer(GL_ARRAY_BUFFER, vbo);
        gl_fn->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        check_gl_errors("create vbo");

        gl_fn->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        gl_fn->glEnableVertexAttribArray(0); 
        check_gl_errors("enable vertex attrib pointer");

        GLuint ebo;
        gl_fn->glGenBuffers(1, &ebo);
        gl_fn->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        gl_fn->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);


        graphics->vao = vao;
        graphics->ebo = ebo;
    }




    // Shaders
    {
        const char *vertex_shader_source =
            "#version 330 core\n"
            "layout (location = 0) in vec3 aPos;\n"
            "uniform vec2 pos;\n"
            "uniform vec2 scale;\n"
            "void main()\n"
            "{\n"
            "    gl_Position = (vec4(aPos.x, aPos.y, aPos.z, 1.0) + vec4(pos, 0.0f, 0.0f)) * vec4(scale, 1.0f, 1.0f) + vec4(-1.0f, -1.0f, 0.0f, 0.0f);\n"
            "}\0";

        const char *fragment_shader_source =
            "#version 330 core\n"
            "uniform vec3 color;\n"
            "out vec4 FragColor;\n"
            "void main()\n"
            "{\n"
            "    FragColor = vec4(color, 1.0f);\n"
            "}\0";

        int  success;
        char info_log[512];

        unsigned int vertex_shader;
        vertex_shader = gl_fn->glCreateShader(GL_VERTEX_SHADER);
        gl_fn->glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
        gl_fn->glCompileShader(vertex_shader);
        check_gl_errors("compile vertex shader");

        gl_fn->glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
        if(!success)
        {
            gl_fn->glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
            printf("ERROR: VERTEX COMPILATION FAILED -\n%s", info_log);
        }

        unsigned int fragment_shader;
        fragment_shader = gl_fn->glCreateShader(GL_FRAGMENT_SHADER);
        gl_fn->glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
        gl_fn->glCompileShader(fragment_shader);
        check_gl_errors("compile fragment shader");

        gl_fn->glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
        if(!success)
        {
            gl_fn->glGetShaderInfoLog(fragment_shader, 512, NULL, info_log);
            printf("ERROR: FRAGMENT COMPILATION FAILED -\n%s", info_log);
        }

        unsigned int shader_program;
        shader_program = gl_fn->glCreateProgram();

        gl_fn->glAttachShader(shader_program, vertex_shader);
        gl_fn->glAttachShader(shader_program, fragment_shader);
        gl_fn->glLinkProgram(shader_program);
        check_gl_errors("link shader program");

        gl_fn->glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
        if(!success) {
            gl_fn->glGetProgramInfoLog(shader_program, 512, NULL, info_log);
            printf("ERROR: PROGRAM LINK FAILED -\n %s", info_log);
        }

        gl_fn->glDeleteShader(vertex_shader);
        gl_fn->glDeleteShader(fragment_shader);  
        check_gl_errors("delete shaders");

        graphics->shader_program = shader_program;
    }

}


void platform_events()
{
    memcpy(graphics->prev_keys_down, graphics->keys_down, sizeof(graphics->keys_down));

    bool size_change = false;

    XEvent ev = {};
    while(XPending(graphics->display) > 0)
    {
        XNextEvent(graphics->display, &ev);
        switch(ev.type)
        {
            case DestroyNotify:
            {
                // Delete ourselves
                XDestroyWindowEvent *e = (XDestroyWindowEvent *) &ev;
                if(e->window == graphics->window)
                {
                    graphics->window_open = false;
                }
            } break;
            case ClientMessage:
            {
                // Recieved "delete ourselves" event
                XClientMessageEvent *e = (XClientMessageEvent *)&ev;
                if((Atom)e->data.l[0] == graphics->wm_delete_window)
                {
                    XDestroyWindow(graphics->display, graphics->window);
                    graphics->window_open = false;
                }
            } break;
            case ConfigureNotify:
            {
                XConfigureEvent *e = (XConfigureEvent *)&ev;
                graphics->window_width = e->width;
                graphics->window_height = e->height;
                size_change = true;
            } break;

            case KeyPress:
            {
                XKeyPressedEvent* e = (XKeyPressedEvent*) &ev;

                if(e->keycode == XKeysymToKeycode(graphics->display, XK_Escape))
                {
                    XDestroyWindow(graphics->display, graphics->window);
                    graphics->window_open = false;
                }
                //printf("Keycode: %d\n", e->keycode);
                graphics->keys_down[e->keycode] = true;
#if 0
                int symbol = 0;
                Status status = 0;
                Xutf8LookupString(graphics->x_input_context, e, (char*)&symbol,
                        4, 0, &status);

                if(status == XBufferOverflow)
                {
                    // Should not happen since there are no utf-8 characters larger than 24bits
                    // But something to be aware of when used to directly write to a string buffer
                    printf("Buffer overflow when trying to create keyboard symbol map\n");
                }
                else if(status == XLookupChars)
                {
                    printf("%s\n", (char*)&symbol);
                }
                if(e->keycode == XKeysymToKeycode(graphics->display, XK_Left)) printf("left arrow pressed\n");
                if(e->keycode == XKeysymToKeycode(graphics->display, XK_Right)) printf("right arrow pressed\n");
                if(e->keycode == XKeysymToKeycode(graphics->display, XK_Up)) printf("up arrow pressed\n");
                if(e->keycode == XKeysymToKeycode(graphics->display, XK_Down)) printf("down arrow pressed\n");
#endif
            } break;
            case KeyRelease:
            {
                XKeyPressedEvent* e = (XKeyPressedEvent*) &ev;

                graphics->keys_down[e->keycode] = false;
#if 0
                if(e->keycode == XKeysymToKeycode(graphics->display, XK_Left)) printf("left arrow released\n");
                if(e->keycode == XKeysymToKeycode(graphics->display, XK_Right)) printf("right arrow released\n");
                if(e->keycode == XKeysymToKeycode(graphics->display, XK_Up)) printf("up arrow released\n");
                if(e->keycode == XKeysymToKeycode(graphics->display, XK_Down)) printf("down arrow released\n");
#endif
            } break;
        }
    }

    if(size_change)
    {
        size_change = false;

#if 0

        XDestroyImage(xWindowBuffer); // Free's the memory we malloced;
        windowBufferSize = width*height*pixelBytes;
        mem  = (char *)malloc(windowBufferSize);

        xWindowBuffer = XCreateImage(graphics->display, visinfo.visual, visinfo.depth,
                ZPixmap, 0, mem, width, height,
                pixelBits, 0);
#endif

        int new_width = (int)((float)graphics->window_height * GRID_ASPECT_RATIO);
        int new_height = graphics->window_height;
        int pad = (graphics->window_width - new_width) / 2.0f;
        glViewport(pad, 0, new_width, new_height);
    }
}

void render()
{
    glClearColor(0, 0, 0.1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    check_gl_errors("clear frame buffer");


    gl_fn->glUseProgram(graphics->shader_program);
    check_gl_errors("use shader program");

    gl_fn->glBindVertexArray(graphics->vao);
    check_gl_errors("bind vao");

    gl_fn->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, graphics->ebo);
    check_gl_errors("bind ebo");

    /*
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    check_gl_errors("draw");
    */

    for(unsigned i = 0; i < graphics->cells_to_render.size(); i++)
    {
        CellData *cell = &graphics->cells_to_render[i];

        v2 pos = v2((float)cell->position.x, (float)cell->position.y);
        v2 scale = v2(2.0f / GRID_WIDTH, 2.0f / GRID_HEIGHT);
        Color color = cell->color;

        GLint loc;
        loc = gl_fn->glGetUniformLocation(graphics->shader_program, "pos");
        //if(loc == -1) printf("Couldnt find uniform\n");
        gl_fn->glUniform2f(loc, pos.x, pos.y);
        loc = gl_fn->glGetUniformLocation(graphics->shader_program, "scale");
        //if(loc == -1) printf("Couldnt find uniform\n");
        gl_fn->glUniform2f(loc, scale.x, scale.y);
        loc = gl_fn->glGetUniformLocation(graphics->shader_program, "color");
        //if(loc == -1) printf("Couldnt find uniform\n");
        gl_fn->glUniform3f(loc, color.r, color.g, color.b);
        //check_gl_errors("set uniform");

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        //check_gl_errors("draw");
    }

    graphics->cells_to_render.clear();

    glXSwapBuffers(graphics->display, graphics->window);
}

bool window_open()
{
    return graphics->window_open;
}

void shutdown_graphics()
{
    glXMakeCurrent(graphics->display, 0, 0);
    glXDestroyContext(graphics->display, graphics->gl_context);

    //XDestroyWindow(graphics->display, graphics->window); // Should be closed in platform_events
    XFreeColormap(graphics->display, graphics->color_map);
    XCloseDisplay(graphics->display);
}




void renderer_add_cell(v2i position, Color color)
{
  CellData cell = {position, color};
  graphics->cells_to_render.push_back(cell);
}

void renderer_add_cell_in_left_bar(v2i position, Color color)
{
}

void renderer_add_cell_in_right_bar(v2i position, Color color)
{
}


void init_input() {}

static int translate_keycode(unsigned char key)
{
    // 24
    //  38
    //   52
    int c = 0;
    switch(key)
    {
        case 'W': { c = 25; break; }
        case 'A': { c = 38; break; }
        case 'S': { c = 39; break; }
        case 'D': { c = 40; break; }
        case 'J': { c = 44; break; }
        case 'L': { c = 46; break; }
        case ' ': { c = 65; break; }
        case 'R': { c = 27; break; }
    }
    return c;
}
bool button_toggled_down(unsigned char key)
{
    int code = translate_keycode(key);
    return (graphics->keys_down[code] && !graphics->prev_keys_down[code]);
}
bool button_toggled_up(unsigned char key)
{
    int code = translate_keycode(key);
    return (!graphics->keys_down[code] && graphics->prev_keys_down[code]);
}

bool button_state(unsigned char key)
{
    int code = translate_keycode(key);
    return graphics->keys_down[code];
}


