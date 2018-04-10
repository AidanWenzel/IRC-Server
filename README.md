Compile with: make

Run with: ./<exe> <password> <port>

Access with: nc <domain> <port>

Simple IRC Server - Aidan Wenzel

Design notes: 
    
    -Need to access users in a channel and all channelMap a user is in, use sets for each
    
    -Create a new thread for each user, share between threads server objects
    
    -Every interaction uses shared resources to check and message different clients, because of this 
    all resources must be mutex locked. Instead of designing a complex locking and unlocking scheme 
    I will just treat all reads and writes as critical. 
    
    -Commands are identified using regex, then additionally verified for correctness. If a given command
    doesn't match any regex it is considered invalid. If it matches regex but fails in length or content 
    then users will receive more specific errors. 
    
    -If users try to message targets they do not have access to they receive an "invalid target" message