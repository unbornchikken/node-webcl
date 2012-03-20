var nodejs = (typeof window === 'undefined');
if(nodejs) {
  cl = require('../webcl');
  log = console.log;
  exit = process.exit;
}

function read_complete(status, data) {
  log('in read_complete, status: '+status);
  log("New data: "+data[0]+', '+data[1]+', '+data[2]+', '+data[3]);
}

function main() {
  /* CL objects */
  var /* WebCLPlatform */     platform;
  var /* WebCLDevice */       device;
  var /* WebCLContext */      context;
  var /* WebCLProgram */      program;
  var /* WebCLKernel */       kernel;
  var /* WebCLCommandQueue */ queue;
  var /* WebCLEvent */        prof_event;
  var /* WebCLBuffer */       data_buffer;
  var /* ArrayBuffer */       mapped_memory;
  
  var NUM_BYTES = 131072; // 128 kB
  var NUM_ITERATIONS = 2000;
  var PROFILE_READ = false; // profile read vs. map buffer
  
  /* Initialize data */
  var data=new Uint8Array(NUM_BYTES);

  /* Create a device and context */
  log('creating context');
  
  //Pick platform
  var platformList=cl.getPlatforms();
  platform=platformList[0];
  log('using platform: '+platform.getInfo(cl.PLATFORM_NAME));
  
  //Query the set of devices on this platform
  var devices = platform.getDevices(cl.DEVICE_TYPE_GPU);
  device=devices[0];
  log('using device: '+device.getInfo(cl.DEVICE_NAME));

  // create GPU context for this platform
  var context=cl.createContext({
    devices: device, 
    platform: platform
  });

  /* Build the program and create a kernel */
  var source = [
    "__kernel void profile_read(__global char16 *c, int num) {",
    "  for(int i=0; i<num; i++) {",
    "     c[i] = (char16)(5);",
    "  }",
    "}"
  ].join("\n");

  // Create and program from source
  try {
    program=context.createProgram(source);
  } catch(ex) {
    log("Couldn't create the program. "+ex);
    exit(1);
  }

  /* Build program */
  try {
    program.build(devices);
  } catch(ex) {
    /* Find size of log and print to std output */
    var info=program.getBuildInfo(devices[0], cl.PROGRAM_BUILD_LOG);
    log(info);
    exit(1);
  }

  try {
    kernel = program.createKernel("profile_read");
  } catch(ex) {
    log("Couldn't create a kernel. "+ex);
    exit(1);   
  }

  /* Create a write-only buffer to hold the output data */
  try {
    data_buffer = context.createBuffer(cl.MEM_WRITE_ONLY, NUM_BYTES);
  } catch(ex) {
    log("Couldn't create a buffer. "+ex);
    exit(1);   
  }

  /* Create kernel argument */
  try {
    kernel.setArg(0, data_buffer);
  } catch(ex) {
    log("Couldn't set a kernel argument. "+ex);
    exit(1);   
  };

  /* Tell kernel number of char16 vectors */
  var num_vectors = NUM_BYTES/16;
  kernel.setArg(1, num_vectors, cl.type.INT);

  /* Create a command queue */
  try {
    queue = context.createCommandQueue(device, cl.QUEUE_PROFILING_ENABLE);
  } catch(ex) {
    log("Couldn't create a command queue for profiling. "+ex);
  };

  var total_time = 0, time_start, time_end;
  prof_event=new cl.WebCLEvent();
  
  for(var i=0;i<NUM_ITERATIONS;i++) {
    /* Enqueue kernel */
    try {
      queue.enqueueTask(kernel);
    } catch(ex) {
      log("Couldn't enqueue the kernel. "+ex);
      exit(1);   
    }

    if(PROFILE_READ) {
      /* Read the buffer */
      try {
        queue.enqueueReadBuffer(data_buffer, false, { 
          buffer: data,
          size: data.byteLength
        }, null, prof_event);
      } catch(ex) {
        log("Couldn't read the buffer. "+ex);
        exit(1);   
      }
    }
    else {
      /* Create memory map */
      try {
        mapped_memory = queue.enqueueMapBuffer(data_buffer, cl.TRUE, cl.MAP_READ, 0, data.byteLength, null, prof_event);
        if(mapped_memory[0]!=5) {
          Throw("Kernel didn't work or mapping is wrong");
        }
      } catch(ex) {
         log("Couldn't map the buffer to host memory. Err= "+ex);
         exit(1);   
      }
    }
    
    /* Get profiling information */
    time_start = prof_event.getProfilingInfo(cl.PROFILING_COMMAND_START);
    time_end = prof_event.getProfilingInfo(cl.PROFILING_COMMAND_END);
    //log("time: start="+time_start+" end="+time_end);
    total_time += time_end - time_start;
    
    if(!PROFILE_READ) {
      /* Unmap the buffer */
      try {
        queue.enqueueUnmapMemObject(data_buffer, mapped_memory);
      } catch(ex) {
         log("Couldn't unmap the buffer. Err= "+ex);
         exit(1);   
      }
    }
  }

  var avg_ns=Math.round(total_time/NUM_ITERATIONS);
  log("Average "+(PROFILE_READ ? "read" : "map")+" time: "+avg_ns+" ns = " +(avg_ns/1000000.0)+" ms");
  
  queue.finish();
  log('queue finished');
}

main();