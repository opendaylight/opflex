policy-agent
============

The Policy Agent (pagentd) will provide policy rendering and policy
enforncement. This is a logical abstraction for a physical or virtual
device that implements and enforces policy. The Policy Agent are
responsible for requesting portions of the policy from the policy authority
as new endpoints connect, disconnect, or change. Additionally, the Policy Agent
are responsible for rendering that policy from an abstract form into a concrete
form that maps to their internal capabilities. This process is a local operation
and can function differently on each device as long as the semantics of the
policy are honored. 

This agent will handle policy as defined in the OpFlex Control
Protocol IETF draft [1]. pagentd is a multi-threaded agent which can 
operate as a PE (Policy Element) according to the draft.


[1] draft-smith-opflex-00c [Link to appear here]

