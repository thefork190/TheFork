# The Fork (in the road)
What is this repo about?

It's an experimental exercise to create a modular and flexible ECS real-time engine.  The overall engine is split into different abstraction "modules".  

# Main Dependencies

## FLECS

Since it is all ECS based, the FLECS library is used as a main dependency for all the engine.  Modularity arises from the usage of FLECS modules.

## The Forge

A trimmed down version of "The Forge" (by The Forge Interactive) is used for cross-platform rendering.

## SDL3

Battle-tested library to get access to hardware and OS events in a cross-platform manner.

# Source Code Structure

Logic starts from the main entry point.  From there, FLECS and The Forge are initialized and a minimal amount of entities are created to kickstart the engine and get the ECS systems working.  From that point on, all logic is purely driven via ECS systems. 

Since everything is data-driven, we split modules into 3 abstraction levels.  Modules at a specific abstraction level cannot rely or depend on components from a higher level.  Modules can only depend on components from a lower level abstraction.

The hope is that such an abstraction will keep modularity, reusability, and flexibility high and maintenance efforts low.  This scheme also makes it easier to quickly prototype systems at a higher level of abstraction and bring them down to a lower level of abstraction once they become more generic and reusable.

## Low Level

The lowest level of abstraction relates to engine related modules.  It provides components and systems that handle the most basic engine logic such as inputs, low-level rendering, application events, window management, presentation and life-cycle, IO logic, etc....

## Middle Level

This level will be composed of modules that handle general application logic.  Things like higher-level rendering logic, UI, content pipeline tooling, etc....  This middle abstraction layer holds commonly reusable systems/components that could be leveraged in many different types of applications.

## High Level

This is where all the application specific logic happens.  All components and systems at this level are very specific to the application.

There could be different high level sets of modules, with each set used for a different application.

# Directories Structure

Source - contains main entry  
Source/Modules/Low - low level modules  
Source/Modules/Middle - middle level modules  
Source/Modules/High/*appname* - high level modules for "appname"  