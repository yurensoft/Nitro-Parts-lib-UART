/* Verilator implementation for sim using python-nitro bindings */

#include <Python.h>
#include "numpy/arrayobject.h"
#include "python_nitro.h"

#include "VUART_tb.h"
#include "VUART_tb_UART_tb.h"
#include "VUART_tb_fx2.h"

#if VM_TRACE
#include "verilated_vcd_c.h"

VerilatedVcdC* tfp = NULL;
bool trace = true;

#else
bool trace = false;
#endif

VUART_tb *tb = NULL;
unsigned int main_time = 0;	// Current simulation time

#define CLK_HALF_PERIOD 11
#define CHECK_INIT   if(tb == NULL) { PyErr_SetString(PyExc_Exception, "You have not initialized this sim yet.  Run init() function"); return NULL; }


double sc_time_stamp () {	// Called by $time in Verilog
    return main_time;
}

void advance_clk(unsigned int cycles=1) {
  while (cycles) {
    // Toggle clock
    if ((main_time % CLK_HALF_PERIOD) == 1) {
      if(tb->clk) {
	tb->clk = 0;
      } else {
	cycles--;
	tb->clk = 1; 
      }
    }

    tb->eval();            // Evaluate model
#if VM_TRACE
    if(trace) tfp->dump (main_time);
#endif
    main_time++;            // Time passes...
  }
}

void open_trace(const char *filename) {
#if VM_TRACE
  tfp->open(filename);
  trace = true;
#endif
}

static PyObject *init(PyObject *self, PyObject *args) {
  const char *filename=NULL;

  if (!PyArg_ParseTuple(args, "|s", &filename)) {
    PyErr_SetString(PyExc_Exception, "Optional argument should be a string specifying the vcd trace filename.");
    return NULL;
  }
    
  tb   = new VUART_tb("tb");	// Create instance of module
  Verilated::debug(0);

#if VM_TRACE
  Verilated::traceEverOn(true);	// Verilator must compute traced signals
  tfp = new VerilatedVcdC;
  tb->trace (tfp, 99);	// Trace 99 levels of hierarchy
  trace = false;
  if(filename) {
    open_trace(filename);
  }
#endif

  // pull fx2 out of reset
  tb->resetb = 0; 
  advance_clk(50);
  tb->resetb = 1; 

  Py_RETURN_NONE; 
}

static PyObject *resetb(PyObject *self, PyObject *args) {
  int reset = -1;
  if (!PyArg_ParseTuple(args, "|i", &reset)) {
    return NULL;
  }
  if(reset == -1) {
    return PyInt_FromLong(tb->resetb);
  } else {
    tb->resetb = (reset) ? 1 : 0; 
    Py_RETURN_NONE; 
  }
}

static PyObject *start_tracing(PyObject *self, PyObject *args) {
#if VM_TRACE
  const char *filename=NULL;
  if (!PyArg_ParseTuple(args, "s", &filename)) {
    PyErr_SetString(PyExc_Exception, "Must provide vcd filename as argument");
    return NULL;
  }
  if(trace) {
    PyErr_SetString(PyExc_Exception, "Tracing is already enabled.");
    return NULL;
  }
  open_trace(filename);
#endif
  Py_RETURN_NONE;
}

static PyObject *time(PyObject *self, PyObject *args) {
  return PyInt_FromLong(main_time);
}

static PyObject *get_dev(PyObject *self, PyObject* args ) {
  if(!tb) {
    PyErr_SetString(PyExc_Exception, "You must call init() prior to this method");
    return NULL;
  }    
  return nitro_from_datatype ( *(tb->v->fx2->fx2_dev) );
}

static PyObject *adv(PyObject *self, PyObject *args) {
  int x;
  CHECK_INIT

  if (!PyArg_ParseTuple(args, "i", &x)) {
    PyErr_SetString(PyExc_Exception, "Argument is number of clock cycles to advance simulation");
    return NULL;
  }
  advance_clk(x);
  Py_RETURN_NONE;
}

static PyObject *end(PyObject *self, PyObject *args) {
  CHECK_INIT
  tb->final();
  delete tb;
#if VM_TRACE
  if(trace) {
    tfp->close();
    delete tfp;
  }
  tfp = NULL;
#endif
  tb = NULL;
  Py_RETURN_NONE; 
}


/*************************************  Vtb extension module ****************/
static PyMethodDef Vtb_methods[] = {
  {"init", init, METH_VARARGS,"Creates an instance of the simulation." },
  {"trace",start_tracing,METH_VARARGS,"Turns on tracing to specified file." },
  {"time", time, METH_NOARGS, "Gets the current time of the simulation." },
  {"adv",  adv,  METH_VARARGS, "Advances sim x number of clk cycles." },
  {"end",  end,  METH_NOARGS, "Ends simulation & deletes all sim objects." },
  {"get_dev", get_dev, METH_NOARGS, "Returns a dev handle." },
  {"resetb", resetb, METH_VARARGS, "Sets resetb" },
  {NULL}  /* Sentinel */
};

#ifndef PyMODINIT_FUNC	/* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC initVUART_tb(void) {
    PyObject* m;

    import_nitro();
    m = Py_InitModule3("VUART_tb", Vtb_methods,
                       "Starter Project Testbench");

    import_array(); // necessary to use numpy arrays
}

//// This is a hack to use the same .cpp file with verilator and have it
//// generate a different python module for gate level sims.
//PyMODINIT_FUNC initVtb_gates(void) {
//    PyObject* m;
//
//    m = Py_InitModule3("Vtb_gates", Vtb_methods,
//                       "Hydrogen Project Testbench For Gate Sims");
//
//    import_array(); // necessary to use numpy arrays
//}
