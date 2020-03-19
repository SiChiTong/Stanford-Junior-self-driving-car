#include "view.h"

void
draw_help( void )
{
#define NUM_KEY_FUNCTIONS  74

  char *help[NUM_KEY_FUNCTIONS] = { 
    "'backspace': unlock camera",
    "'space': show obstacles",
    "'tab': cycle cars / GLS",
    "'|': cycle window size",
    "'0': inverse color scheme",
    "'3': show ld-lrs laser 1",
    "'4': show ld-lrs laser 2",
    "'#': show lms laser 3",
    "'$': show lms laser 1",
    "'6': show all sensors",
    "'7': show shm grid map",
    "'8': read transformations",
    "'9': show lines",
    "'(': show beams",
    "'-': mark previous beam",
    "'=': mark next beam",
    "'a': show sensor info",
    "'A': move sensor window",
    "'b': show radar",
    "'B': show radar cal. data",
    "'c': draw colors",
    "'C': change colors",
    "'d': show distance rings",
    "'e': show info",
    "'E': show status",
    "'f': follow mode",
    "'F': change follow viewpoint",
    "'g': show grid",
    "'h': show help",
    "'i': show imagery",
    "'I': change imagery type",
    "'j': show actuators",
    "'k': show inside view",
    "'l': show lower block",
    "'m': show ipc grid map",
    "'n': show rndf",
    "'o': request map",
    "'p': plain mode",
    "'q': quit program",
    "'r': record video (on/off)",
    "'s': show dynamic (tracking)",
    "'t': show trajectory", 
    "'T': show planner goal", 
    "'u': show upper block",
    "'v': switch car / cage",
    "'V': show car",
    "'w': mark single beam",
    "'x': show intensity",
    "'y': show colored laser beams",
    "'z': use large points",
    "'z': use huge points",
    "',': show shm data",
    "'~': reset map",
    "'`': show car marker",
    "'[': show gls",
    "']': show simulator",
    "'CTRL+A': show applanix info",
    "'CTRL+B': send goal point",
    "'CTRL+C': show laser colors by height",
    "'CTRL+D': show estop",
    "'CTRL+E': unlimited camera angle",
    "'CTRL+F': show cross track error",
    "'CTRL+G': set goal point",
    "'CTRL+J': show can data in actuator",
    "'CTRL+L': show localize",
    "'CTRL+M': show stickered car",
    "'CTRL+O': show coordinates",
    "'CTRL+P': set car in PAUSE mode",
    "'CTRL+R': set car in RUN mode",
    "'CTRL+S': set car in PAUSE mode",
    "'CTRL+U': show clock",
    "'CTRL+V': show g-meter",
    "'CTRL+W': filter radar data",
    "'CTRL+Y': sync ghostcar",
  };
  vlr::gui3D_help( NUM_KEY_FUNCTIONS, help, GLUT_BITMAP_HELVETICA_18 );
}
