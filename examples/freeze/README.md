# Module flash loading aka freezing
MicroPythonRT introduces the ability to load Python modules directly into flash instead of RAM. This operation is customarily referred to as "freezing" and can signficantly reduce the RAM usage of a program.

## Before/after freezing comparison
Below is example of how to use freezing which measures its effect on RAM usage.
```
MicroPython v1.21.0-1-gf489ec2f12-dirty on 2023-10-12; Raspberry Pi Pico with RP2040
Type "help()" for more information.
>>> import freeze, gc, micropython
>>> gc.collect()
>>> micropython.mem_info()
stack: 500 out of 7936
GC: total: 112000, used: 6880, free: 105120
 No. of 1-blocks: 19, 2-blocks: 36, max blk sz: 265, max free sz: 6368
```
Here we have a freshly booted MicroPython system. We run a garbage collection just to make sure there is no garbage lying around and then check the state of the heap. There are 6880 bytes of memory in use by the heap.

```
>>> import casyncio as asyncio
>>> gc.collect()
>>> micropython.mem_info()
stack: 500 out of 7936
GC: total: 112000, used: 83872, free: 28128
 No. of 1-blocks: 398, 2-blocks: 869, max blk sz: 265, max free sz: 54
```
Next we import the [`casyncio`](/lib/micropython-lib/python-stdlib/casyncio/) module. This is a huge module. After cleaning up garbage and checking the heap again, we see that 83872 bytes are in use. This library just used up 75 kB of our device's precious RAM, and we haven't started running any code yet!

```
>>>
MPY: soft reboot
MicroPython v1.21.0-1-gf489ec2f12-dirty on 2023-10-12; Raspberry Pi Pico with RP2040
Type "help()" for more information.
>> import freeze, gc, micropython
>> freeze.import_modules('casyncio')
froze 68876 flash bytes, 560 ram bytes, 1369 objects, 512 qstrs
```
Next we reboot to start with a clean heap and import `casyncio` via the `freeze` module. It reports that it froze (i.e., put into flash) 68876 bytes of memory. This is pretty much all of the 75 kB that module import produced because the 68876 bytes here is "compacted" compared to what `mem_info` reports. It was only unable to freeze 560 bytes of the 75 kB due to mutable state (see below).

```
>>> import casyncio as asyncio
>>> gc.collect()
>>> micropython.mem_info()
stack: 500 out of 7936
GC: total: 112000, used: 6800, free: 105200
 No. of 1-blocks: 21, 2-blocks: 35, max blk sz: 265, max free sz: 6318
```
Now we can import the `casyncio` module for real. Finally we do a GC and `mem_info` again to see the results. Now there are only 6800 bytes in use: the same as when we started even though now the `casyncio` module is loaded and ready to use. 75 kB of RAM has been freed up and available to the program to use again. Not only does this free up RAM, but it also enables the GC heap to be smaller, thus making garbage collections faster.

### How to delete frozen modules
```
>>> freeze.clear()
>>>
MPY: soft reboot
```
If you make changes to the source code of your frozen modules, you need to manually re-freeze them. Call `clear` to clear the frozen modules. You can then re-import your modules to re-freeze them.

### A note about heap size
The default heap size for MicroPythonRT on RP2 is 96 kB. That's actually not enough RAM to be able to import the `casyncio` module!
```
>>> import casyncio as asyncio
Traceback (most recent call last):
  File "<stdin>", line 1, in <module>
  File "./__init__.py", line 58, in <module>
MemoryError: memory allocation failed, allocating 776 bytes
```
So the code above was run with a heap size of 112 kB just to allow the module to load in RAM and compare sizes. However one of the benefits of freezing is that it is possible to freeze a module that is too big to fit in RAM! So with the default heap size of 96 kB, it is possible to freeze and import the `casyncio` module even though it cannot be imported directly.

## How does freezing work?
To understand this we first need to understand how MicroPython runs code.

### How is MicroPython code executed?
The MicroPython compiler (mpy-cross) is responsible for converting Python (.py) source file into a .mpy files. You can run mpy-cross yourself on your desktop and only copy the output .mpy files to your device. This saves some filesystem space as .mpy files are typically smaller.

When you import a module from Python code running on your device, MicroPython looks for a .py or .mpy file with the same name as the module. If it is a .py file, it runs the compiler on device to produce an .mpy file. Either way it ends up with a .mpy file to continue loading with.

Unlike .so or .dll files, .mpy files are not loadable memory images that are ready to run. Instead they are more like a binary format of the same information that was in the .py file. So to import a module, the MicroPython runtime starts executing the top-level Python code of the module's source file. Typically this code creates the classes, functions, and global variables of the module. Executing this code creates objects in the MicroPython heap to represent the module, classes, functions, etc. It is these allocations that constitute the RAM cost of loading a module. 

For example, loading this Python file, would create in-memory data structures similar to the following diagram.

```example.py```
```
class Foo:
  def Bar(self):
    return 0
``````
![obj](/examples/freeze/mp_freeze.svg)

First there is a module object. This objects contains info about the module such as its name, but mainly a pointer to a dictionary object that contains all of the members of the module. This dictionary has one entry associating the name "Foo" with a type object. Similarly the type object has some info fields but mainly a pointer to another dictionary for the members of the class. This dictionary has one entry associating the name "Bar" with a function object. Finally this function object contains the bytecode for the function. There are more details to this than what is presented here, but this is the gist of it.

Surprisingly all these objects created in memory to represent the module plus the function bytecode typically take up more space than the original .mpy file did in flash.

### How does freezing help?
The key to reducing memory usage of loaded Python modules is not so much about doing something with .mpy files, but rather reducing the objects that are stored in RAM after the .mpy file has been loaded. For comparison, native modules don't have a RAM footprint because all of their module data structures are hard-coded as `const` globals in C code, which means they are stored in flash and never loaded into RAM. We would like to be able to do the same thing with user modules: store all our module data structures in flash instead of RAM, and do so at run-time, without having to recompile the firmware binary.

MicroPythonRT achieves this using a technique similar to a relocating garbage collector. Starting at a module object newly loaded into RAM, we traverse its object graph and relocate all the objects to flash memory space. Once the object graph has been relocated, the original copy in RAM is no longer needed and can be garbage collected.

In order to relocate objects, we have to make some assumptions, the biggest of which is that all the objects are immutable. This is pretty much true for all the "metadata" objects we are interested in freezing. Typically, objects representing modules, classes, and functions don't change after they are created. Although technically they can change, as nothing stops later code from adding a new memeber to a module or class. MicroPython users are presumably familiar with this limitation since native modules already cannot be mutated. In many scenarios, not mutating modules is a fair trade-off for the amount of RAM that is saved.

### How is shared mutable state handled? 
To faciliate module freezing, shared mutable state such as mutable global variables or mutable class attributes should be avoided. However in the real world, it can never be avoided completely so we have to deal with it somehow. When the object graph relocation traversal finds a mutable object (such as a `list` or user-defined class) it relocates it to a special section of RAM. This section has a fixed address, is outside of the GC heap, but acts as a root for garbage collection.

![text](/examples/freeze/mp_freeze_2.svg)
When the device is off it is in the persisted state. All of the immutable frozen objects are stored in flash along with the initial state of the mutable frozen objects. When MicroPython starts running on the device, the mutable frozen objects are copied into RAM and their initial state on flash is no longer needed. The GC heap is also created in RAM and the system list of modules is setup up to point to the frozen modules in flash. Since the mutable frozen objects are in RAM, they can be mutated, and potential point to other objects in the GC heap. However the mutable frozen objects themselves can never be garage collected and there special RAM section is permanently allocated.

Below is an example of how to use mutable global variables in frozen modules.
```
class _Counter:
  value = 0

_counter = _Counter()

def get_next():
  _counter.value += 1
  return _counter.value
```
`get_next` is a function that just returns the next integer in sequence. The `_Counter` class acts as a holder for the mutable value. The freezing machinery sees `_counter` as a mutable object so places it in the frozen RAM section where it can be mutated by the function `get_next`.

In order for this system to be worthwhile, the number of mutable frozen objects has to be orders of magnitude smaller than the number of immutable frozen objects, and fortunately in practice they typically are. This is why it is still important to minimize the use of shared mutable state.
