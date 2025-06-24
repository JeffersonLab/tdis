# Implementation Concepts

[approach.md](approach.md ':include')

## ACTS possibilities 

ACTS provides a whole chain of tracking tasks, such as:
- digitization, 
- clustering, 
- seeding, 
- track fitting, 
- vertexing, 
- calibration and alignment
- various tools, tunings, data model etc. etc. etc. 

It also usually has multiple algorithmic options for each of these tasks,
such as "what seeding algorithm to use?", "what fitting algorithm to use?",
"do we use forward, reverse, or both-way passes?". 

But also, it utilizes machine learning and AI elements, 
such as AI for tracking, which can optionally be turned on during the later stage 
of the software development.

Implementing full ACTS possibilities for TDIS experiment is such 
multi-modal-complexity tasklist, that cannot be implemented easily in place at once. 
So, we decided which steps are important to start with and which steps should be implemented next. 
In particular, we decided that:

- The first step we need to do to implement correctly working track fitting. 
- And after this, move to the track finding. 
- After this, having track finding and track fitting correctly working in the modes close to what we expect from the experiment, move to the vertexing and other options.


To implement this, we initially will use truth seeding. Here it is explained. 
Since seeding is not going to be implemented in the first order,
but track fitting algorithms need some seeding;
initially we will use smeared initial truth track parameters
as a seeding input for the fitting.

Here we come to an important point.
What data is required for ACTS in order to do these first major steps, such as seeding, track-finding, and vertexing?

## Inputs and outputs

As of now we can divide the information needed in several categories:

- information of hits
- information on magnetic fields, 
- and we need information about the geometry.
  - geometry related to hits
  - geometry needed to create material map

In future, other information such as calibration and alignment constants will be also needed.

[Tracking in a nutshell](https://acts.readthedocs.io/en/latest/tracking.html) has 
a good explanation about the interconnection between these categories. 

[ACTS documentation on Event Model](https://acts.readthedocs.io/en/latest/core/eventdata/index.html): 

![Data Flow Diagram](https://acts.readthedocs.io/en/latest/_images/edm_chain.svg)

## Track fitting


It is very important feature or limitation of ACTS package 
is that it is thus fitting based on some planes, 2D planes. 

Which means that all hit information has to be provided as 2D information on some geometrical planes. 

This means that first we need geometry to convert from our 3D world hits coordinates to some kind of geometry planes and 2D coordinate of hits on these planes. 

For many tracking detectors, such as, for example, pixel detectors, this conversion is pretty straightforward, as you have a tracking-sensitive plane and you have elements on it. 

But things are not such straight forward for detectors such as TPC detectors. 

In particular, multi-TPC chambers for TDIS experiment. For such type of detectors to make acts library work, you have to use some kind of virtual planes to convert real 3D position of hit to hit from some plane.

For TDIS geometry consisting of discs and discs consisting of rings and rings consisting of individual parts, we use cylinder geometry representation for ACTS. Cylinders go in Z direction. And their radius corresponds to the center of pads.

---

## Links to mentioned packages:


- **ACTS (A Common Tracking Software)**: https://acts-project.github.io/

- **TDIS (Jefferson Lab repository)**: https://github.com/JeffersonLab/tdis

- [GitHub](https://github.com/JeffersonLab/tdis ':target=_blank')

### C++ ACTS + JANA2 based tracking for TDIS experiment



## Core Architecture

![JANA diagram](_media/jana-flow.svg)


At its core, JANA2 views data processing as a chain of transformations, 
where algorithms are applied to data to produce more refined data. 
This process is organized into two main layers:

1. **Queue-Arrow Mechanism:** JANA2 utilizes the [arrow model](https://en.wikipedia.org/wiki/Arrow_\(computer_science\)), 
   where data starts in a queue. An "arrow" pulls data from the queue, processes it with algorithms, 
   and places the processed data into another queue. The simplest setup involves input and output queues 
   with a single arrow handling all necessary algorithms. But JANA2 supports more complex configurations 
   with multiple queues and arrows chained together, operating sequentially or in parallel as needed.

   ![Queue-Arrow mechanism](_media/arrows-queue.svg)

2. **Algorithm Management within Arrows:** Within each arrow, JANA2 organizes and manages algorithms along with their
  inputs and outputs, allowing flexibility in data processing. Arrows can be configured to distribute the processing
  load across various algorithms. By assigning threads to arrows, JANA2 leverages modern hardware to process data 
  concurrently across multiple cores and processors, enhancing scalability and efficiency.

In organizing, managing, and building the codebase, JANA2 provides:

- **Algorithm Building Blocks:** Essential components like Factories, Processors, Services and others, 
  help write, organize and manage algorithms. These modular units can be configured and combined to construct 
  the desired data processing pipelines, promoting flexibility and scalability.

- **Plugin Mechanism:** Orthogonal to the above, JANA2 offers a plugin mechanism to enhance modularity and flexibility. 
  Plugins are dynamic libraries with a specialized interface, enabling them to register components with the main application.
  This allows for dynamic runtime configuration, selecting or replacing algorithms and components without recompilation,
  and better code organization and reuse. Large applications are typically built from multiple plugins, 
  each responsible for specific processing aspects. Alternatively, monolithic applications without plugins 
  can be created for simpler, smaller applications.


## Building blocks

The data analysis application flow can be viewed as a chain of algorithms that transform input data into the 
desired output. A simplified example of such a chain is shown in the diagram below:

![Simple Algorithms Flow](_media/algo_flow_01.svg)

In this example, for each event, raw ADC values of hits are processed: 
first combined into clusters, then passed into track-finding and fitting algorithms, 
with the resulting tracks as the chain's output. In real-world scenarios, 
the actual graph is significantly more complex and requires additional components such as Geometry, 
magnetic field maps, calibrations, alignments, etc. 
Additionally, some algorithms are responsible not only for processing objects in memory 
but also for tasks such as reading data from disk or DAQ streams 
and writing reconstructed data to a destination. 
A more realistic and complex flow can be represented as follows:


![Simple Algorithms Flow](_media/algo_flow_02.svg)

To give very brief overview algorithm building blocks, how this flow is organized in JANA2 : 

- **JFactory** - This is the primary component for implementing algorithms (depicted as orange boxes). 
  JFactories compute specific results on an event-by-event basis. 
  Their inputs may come from an EventSource or other JFactories. 
  Algorithms in JFactories can be implemented using either Declarative or Imperative approaches 
  (described later in the documentation).

- **JEventSource** - A special type of algorithm responsible for acquiring raw event data, 
  and exposes it to JANA for subsequent processing. For example reading events from a file or listening 
  to DAQ messaging producer which provides raw event data.  

- **JEventProcessor** - Positioned at the top of the calculation chain, JEventProcessor is designed 
  to collect data from JFactories and handle end-point processing tasks, such as writing results to 
  an output file or messaging consumer. However, JEventProcessor is not limited to I/O operations; 
  it can also perform tasks like histogram plotting, data quality monitoring, and other forms of analysis.

  To clarify the distinction: JFactories form a lazy directed acyclic graph (DAG), 
  where each factory defines a specific step in the data processing chain. 
  In contrast, the JEventProcessor algorithm is executed for each event. 
  When the JEventProcessor collects data, it triggers the lazy evaluation of the required factories, 
  initiating the corresponding steps in the data processing chain.

- **JService** - Used to store resources that remain constant across events, such as Geometry descriptions, 
 Magnetic Field Maps, and other shared data. Services are accessible by both algorithms and other services.


We now may redraw the above diagram in terms of JANA2 building blocks:

![Simple Algorithms Flow](_media/algo_flow_03.svg)


## Data model

JANA2 alows users to define and select their own event models,
providing the flexibility to design data structures to specific experimental needs. Taking the above
diagram as an example, classes such as `RawHits`, `HitClusters`, ... `Tracks` might be just a user defined classes.
The data structures can be as simple as:

```cpp
struct GenericHit {
double x,y,z, edep;
};
```

A key feature of JANA2 is that it doesn't require data being passed around 
to inherit from any specific base class, such as JObject (used in JANA1) or ROOT's TObject. 
While your data classes can inherit from other classes if your data model requires it, 
JANA2 remains agnostic about this. 

JANA2 offers extended support for PODIO (Plain Old Data Input/Output) to facilitate standardized data handling,
it does not mandate the use of PODIO or even ROOT. This ensures that users can choose the most suitable data management
tools for their projects without being constrained by the framework.



