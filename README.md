# Queuing simulation in Computer Network
This code simulates three different queuing systems in a Computer network:  
Assume that three types of sources generate bits stream with certain rates.  
Each source has its own ON and OFF time, which comply with exponential distribution probability.  
The serving capacity is 10 Mbps, meaning the server will process it with a bandwidth of 10M bits when the packet comes.  

- **FIFO**, First In First Out
  1. This system has a queue with the size of 3MB, first come, first serve.  
  2. Packets arriving when the queue is full will be discarded.
    
- **SPQ**,Strict Priority Queueing
  1. Three queues with the size of 1MB for each of them.  
  2. The total size of these three queues equals the FIFO.
  3. Different types of packets are put into different queues.
  4. Packets are dropped when the queue they should be in is full. 
     
- **WFQ**,Weighted Fair Queuing
  1. Queues in this system are the same as SPQ.
  2. The difference is that each queue has different pre-weights indicating its priority (0.5, 0.3, 0.2).
  3. The percentage of total bits served from the three queues should meet the weighted defined above.
  4. The real-time weights are calculated.  
  5. Real-time weights decide which queue should be served first so that the actual weight of each queue approaches the preset weights.

- The details of each type of source are defined below

Source Type | Peak bit rate (kbps) | Mean on-time (sec) | Mean off-time (sec) | Packet size (bytes)| Number of Sources|
-------|-------|-------|-------|-------|-------|
Audio| 64| 0.36| 0.64 |120 | Na = variable |
Video| 384| 0.33 |0.73 |1000| Nv = variable |
Data | 256 | 0.35 | 0.65 | 583 | Nd = variable |

- Use the number of each source to control the **offered load**.
