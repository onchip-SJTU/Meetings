# 1 Optimization of Interconnects Between Accelerators and Shared Memories in Dark Silicon

## 1 研究目的

​    加速器（accelerator）之间的数据传输需要高速传输，而共享内存（Shared Memories）是一种减小数据传输时间的方式，本文主要研究如何设计Shared Memories与accelerator之间的连接，使得既能够提高数据传输速率，又可以节约硬件资源。   

​    本文主要根据三个方面进行连接的优化：   

#### 1.1 相同加速器的多个数据端口一起通电/断开

​    由于同一个accelerator的多个数据端口（data port) 基本同时通电或者断开，这样一个accelerator和一个memory bank之间只需要一个switch进行连接，节省了switch的数量。   

#### 1.2 暗硅时代芯片可以同时支持的加速器的有限的

​    如题，芯片可以同时power的加速器个数是有限的，本文提供了三个设计使得switch的数量逐渐减少：

##### 1.2.1 100%路由性能下的最小设计

   这种设计是满足memory bank的数量恰好等于最大能够power的accelerator的个数c与每个accelerator的data port数量（这里假设都为n）之积（n*c)。

##### 1.2.2 100%路由性能下且并非最小memory bank数量的设计

   这种设计是满足memory bank的数量大于最大能够power的accelerator的个数c与每个accelerator的data port数量（这里假设都为n）之积（n*c)，这个时候可以减小switch的数量，实现硬件优化。

##### 1.2.3 95%路由性能下的设计

  路由性能指的是各种c个accelerator的组合可以实现的概率，而实际上有些路由组合是用不到的，所以可以通过去掉这些组合，达到95%的路由性能，从而减小switch的数量。

#### 1.3 加速器的异构性

  accelerator之间的相关性在图（a）中显示，相关性越高，越有可能同时被power，所以在设计的时候，可以将这些accelerator的data port与memory bank的连接进行分散化分配，如图（b）所示。而这是借助accelerator的异构性减小了switch的数量。

![](./1.1.png)

## 2 有价值的idea

  本文主要是讲述如何减小switch的数量，可以在确定使用shared memory优化accelerator数据传输后，用来减小switch的数量。

## References

[1] J. Cong and B. Xiao, "Optimization of interconnects between accelerators and shared memories in dark silicon," 2013 IEEE/ACM International Conference on Computer-Aided Design (ICCAD), San Jose, CA, 2013, pp. 630-637.



# 2 Interconnect Synthesis of Heterogeneous Accelerators in a Shared Memory Architecture

## 1 研究目的

  本文主要是从accelerator与shared memory和外存储器的链接两个方面提供了相应的网络设计算法与架构，实现数据的快速传输。如下图所示：

![image-20200320225101415](./2.1.png)

#### 1.1 Optimal partial crossbar

  这是在accelerator与shared memory之间的连接。提出了memory bank对应accelerator的各个data port的分配算法，与上一篇文章不同的是，这里的每个accelerator的data port数量并不一定相等。算法的伪代码如下图：

![image-20200320225704251](./2.2.png)

以图（a)为例说明代码含义：

![image-20200320225931226](./2.3.png)

假设异构accelerator共有8个，最多能够power的数量为3个，首先对于accelerator按照data port数量d进行从大到小排序，然后将最多容纳的3个accumulator和memory bank进行一对一分配，如图中a1，a2，a3，并分成了3个region。然后将剩下的5个accelerator进行一对3分配，即在每个region内重新一轮分配。

根据这种算法，最终得到的switch的数量为：

![image-20200320230439752](./2.4.png)

#### 1.2  Interleaved network

第二部分是Interleaved network，用于memory bank与外存储器之间的连接，用来缓解在DMAC的conflicts，实现数据的快速传输：

![image-20200320230802196](./2.5.png)

## 2 有价值的idea

提出了怎样去连接accelerator和memory bank、外存储器，实现数据的有效传输,减小了switch的数量，可以在确定使用shared memory优化数据传输时提供具体的分配算法。

## References

[2]Y. Chen and J. Cong, "Interconnect synthesis of heterogeneous accelerators in a shared memory architecture," 2015 IEEE/ACM International Symposium on Low Power Electronics and Design (ISLPED), Rome, 2015, pp. 359-364.



# 3 On-chip Interconnection Network for Accelerator-Rich Architectures

## 1 研究目的

与通用型processor相比，加速器可以提高几个数量级的性能，但是加速器的吞吐量常常受到加速器与内存系统数据传输速率的限制，这就增加了对芯片上网络(NoC)设计的需求。

在这篇论文中，提出了混合网络预测预留的架构：

（1）提出了基于位置和精确时间周期的加速电路交换路径的全局管理方法。

（2）提出了混合网络在预留时间内提供传输预测，从而减少包交换而造成的干扰。

本文主要是介绍the Hybrid network with Predictive Reservation (HPR)。

#### 1.1 Baseline architecture

##### 1.1.1 总体网络架构

本文的基本结构如下图所示：

![image-20200321000439823](./3.1.png)

主要由三部分组成：

1) on-chip accelerators   实现复杂的专门固定功能

2) buffer in NUCA   缓存

3) a global accelerator manager   协助进行动态负载平衡和任务调度

##### 1.1.2  加速器组成

![image-20200321001320753](./3.2.png)

其中，DMA负责在SPM和共享的L2缓存之间传输数据，在SPM之间传输数据， 

全局加速器管理器(GAM):

1)它提供了一个软件调用加速器的单一接口，

2)提供共享资源管理，包括缓冲区分配和多个加速器之间的动态负载平衡。

#### 1.2  Global Management of Circuit-Switching

   GAM能够在将任务分配给加速器之前，从核心发送的任务描述中获取有关加速器类型和读/写集的信息。因此，可以很容易地获得加速器的延迟，并且可以通过执行地址转换来提取可能的读/写位置。根据这些信息，GAM可以知道通信的时间，而不必实际执行任务。为了防止路径保留中的冲突，GAM使用一个全局保留表来跟踪网络中每个路由器的当前保留状态。由此，可以将各个任务进行排序，分配不同的路由，实现circuit-switching。   

主要算法如下图所示：

![image-20200321001751545](./3.3.png)

#### 1.3  Hybrid Network

Hybrid Network就是在network中不仅有circuit-switching还有packet-switching，如下图所示：

![image-20200321001923010](./3.4.png)

在一个保留的电路交换会话开始之前，路由器需要检查传入的flit的电路字段，以确定数据流是否到达。一旦确认了数据流，路由器就会将传入的flits直接转发到已经根据预约配置好的switch。在会话结束之前，缓冲区中的包交换flits不允许执行虚拟信道分配(VA)和开关分配(SA)。如果电路字段为零，则说明这是一个包交换flit，电路交换结束，开始包交换。  

## 2 有价值的idea

本文提出的网络主要是基于NoC的混合网络，运用accelerator中的GEM 对不同的任务进行时间预测，并运用混合网络实现circuit-switching和packet-switching，实现减少时间的作用。如果想要实现基于NoC的优化，可以使用这种方法。

## References

[3]J. Cong, M. Gill, Y. Hao, G. Reinman and Bo Yuan, "On-chip interconnection network for accelerator-rich architectures," 2015 52nd ACM/EDAC/IEEE Design Automation Conference (DAC), San Francisco, CA, 2015, pp. 1-6.   



# 4 Hybrid Interconnect Design For Heterogeneous Hardware Accelerators

## 1 研究目的

本文首先列举了基于Bus的网络架构的例子，然后是对于异构芯片提出了使得效率增加、数据传输时间减少的设计方法：

（1）A heuristic-based and detailed profile-driven interconnect design

（2）An automated hybrid interconnect design

#### 1.1 基于Bus的网络架构

accelerator与CPU不同的地方就是数据的throughput比较大，可以从以下四个方面对于数据传输速率进行优化：

1）Bus-based interconnect
2）NoC-based interconnect
3）Shared memory
4）Crossbar

它们产生的影响和优缺点如下图所示：

![image-20200325111142790](./4.1.png)

基于Bus的结构，如下图所示，

<img src="./4.2.png" alt="4.2" style="zoom:67%;" />


假设两个kernel之间的数据传输是必须经过host processor，这样会消耗大量的时间，因此将Bus与其他的结构结合可以改进：

1）与DMA混合：两个kernel的local memory之间的数据传输使用DMA进行存取，节省了从Host processor传输的时间；

2）与crossbar混合：两个kernel的local memory之间的数据传输使用crossbar进行存取；

3）与DMA和crossbar同时混合：两个kernel的local memory之间的数据传输使用crossbar进行存取，其他的kernel可以使用DMA进行数据传输；

4）与NoC混合。

#### 1.2 A heuristic-based and detailed profile-driven interconnect design

由于运行的application是不一样的，所以使用的interconnect也是不同的，根据不同的application产生的数据传输图以及数据流量可以提出一种自发性的基于detailed profile（就是数据传输图）的interconnect。

本文首先提出了四种不同的解决方案，然后设计了一种算法，根据不同application的QUAD toolset（就是detailed profile），选择不同的解决方案：

1）Solution 1 Crossbar-based shared local memory
2）Solution 2 DMA support for parallel processing
3）Solution 3 Local buffer                                                                                                                                                4）Solution 4 Hardware accelerator duplication

算法如下图：

<img src="./4.3.png" alt="4.3" style="zoom:67%;" />


这种算法主要是在embedded platform上面使用。

#### 1.3 An automated hybrid interconnect design

这种方法主要是基于shared memory和NoC，shared memory根据有没有crossbar可以分成两类，而NoC将kernel和local memory分别对应到网络中,在设计interconnect的时候会优先选择crossbar，因为NoC的硬件开销比较大，主要的算法如下图所示：

<img src="./4.4.png" alt="4.4" style="zoom:67%;" />

在将kernel和local memory映射到NoC的过程中不一定需要将所有的都映射上去，因为有些组合是不可以实现的，基于这个可以减少NoC中的硬件资源。 

## 2 有价值的idea

本文比较系统的介绍了减少kernel之间数据传输时间的方式，基于Bus,NoC以及根据不同application选择interconnect的算法，如果选用NoC可以参考本文。

## References

[4]Pham-Quoc, Cuong. (2015). Hybrid Interconnect Design for Heterogeneous Hardware Accelerators. 10.4233/uuid:862e5b58-b9d1-462a-90b0-6f94a054632e. 





# 5 Time-Division-Multiplexing Based Hybrid-Switched NoC for Heterogeneous Multicore Systems

## 1 研究目的

主要是两个方面，第一是基于NoC的circuit-switching和packet-switching混合网络传输数据，减少数据传输时间；第二是基于token network的减少时延提高内存存取数据效率，通过token的标志让memory数据存取排序有序，节省时间。

#### 1.1 **TDM-based Hybrid-Switched Network**

##### 1.1.1 网络结构

混合网络主要是硬件上将circuit-switching和packet-switching的结构同时存在，将时间分成不同的时间slot，在不同的时间进行不一样的数据传输，针对CPU一般会选择packet-switching，而GPU之类的accelerator选用hybrid -switching,如下图所示：

<img src="./5.1.png" alt="5.1" style="zoom:67%;" />


##### 1.1.2 网络的优化

由于circuit-switching建立连接之后省去路由的时间，所以根据以下两种情况对链路进行共享：

1）Hitchhiker-Sharing

当新建立的链路的目的节点与之前相同时，实现链路共享，其中链路数据传输的时间会存储在每个节点的Destination Lookup Table中。

2）Vicinity-Sharing

当新建立的链路的目的节点在原目的节点的附近时，也可以共享链路资源。

#### 1.2 **Memory Access Ordering Network**

这个网络是针对将memory连接在NoC上面的情况，和上一篇中local memory、kernel同时连接在network是一样的，但是为了减少内存访问时间，需要避免因为request被处理的排序导致的问题，这个时候就提出了这样一个内存存取排序网络，主要是由1.1中的混合网络、token ring network、network interface (NI) 构成。

##### 1.2.1 token ring network

token标志着request被处理的顺序的信息，会在token网络中传输。首先是token的形式，记载着每个core上面正在被处理的request的id，以及每个ordering节点的完成情况。当ordering节点完成操作之后，Mi就会被置为1。token会在token network中传输，当network interface需要的时候会取出来使用。

<img src="./5.2.png" alt="5.2" style="zoom:67%;" />

##### 1.2.2 network interface

network interface（NI）在每个ordering point接收并检查token，确保每个ordering 节点处理的request必须是比token中标志的request的id要小。

1）发送过程：当预留的time slot准备好的时候，有序的request会被注入网络中，当网络空闲的时候，无序的request会被传输。

2）接收过程：当无序的request被接收的时候，会按照FIFO的顺序进行排列；有序的request会在每个core的re-order array中排列，当token buffer中的对应的core的req_id的flag为1的时候，request会被按照顺序处理。

3）token的处理过程：当buffer中的flag被置为1的时候，就会开始处理对应的request。

![image-20200325111142790](./5.3.png)

## 2 有价值的idea

主要是两个方面，第一是基于NoC的circuit-switching和packet-switching混合网络传输数据，第二是基于token network的减少时延提高内存存取数据效率。如果是基于NoC的网络互连可以用本文方法设计互联网络和内存存取网络。

## References

 [5]Yin, J. . (2015). Time-division-multiplexing based hybrid-switched noc for heterogeneous multicore systems. *Dissertations & Theses - Gradworks*.





# **6 Hybrid On-Chip Communication Architectures for Heterogeneous Manycore Systems** 

## 1 研究目的

本文主要是对于CPU和GPU的多核系统提出的改进方案，使得CPU与MC（memory controller）之间的延迟最小，GPU与MC之间的throughput的对时间的影响最小。   

主要提出了三个改进方案：

1）ML-enabled NoC design optimization

2）Three-Dimensional (3D) NoC

#### 1.1  数据传输模式

片上的组成如下图所示，少量的CPU和MC，大量的GPU：

<img src="./6.1.png" alt="6.1" style="zoom:67%;" />

其中CPU是latency-sensitive，GPU是throughput-sensitive，而在数据传输方面，GPU-MC的数据传输是最多的：

<img src="./6.2.png" alt="6.2" style="zoom:67%;" />

#### 1.2 ML-enabled NoC design optimization

将新提出的MOO-STAGE与传统的多目标优化AMOSA进行对比，MOO-STAGE提出一种选取初始值的方法，从之前的搜索轨迹中学习一个函数，来判断最佳初始值，使得性能得到优化，energy与latency乘积减小。

#### 1.3 Three-Dimensional (3D) NoC

基于无线NoC提出了3D-NoC的结构，将GPU、CPU、MC放在不同的层上，合理设计减少energy:

<img src="./6.4.png" alt="6.4" style="zoom:67%;" />

## 2 有价值的idea

本文提出了基于GPU和CPU不同特点的architecture设计，为今后片上是这种core的组合提供了方案，同时还提出了8种用来测试NoC的性能。

## References

[6]Joardar, Biresh & Doppa, Janardhan & Pande, Partha & Marculescu, Diana & Marculescu, Radu. (2018). Hybrid on-chip communication architectures for heterogeneous manycore systems. 1-6. 10.1145/3240765.3243480. 





# 7  Architecture Support for Accelerator-Rich CMPs  

## 1 研究目的

本文主要是提出了一种基于NoC的ARC模型，如下图所示，运用GAM来实现accelerator的调度，实现accelerator的共享与组合。

<img src="./7.2.png" alt="7.2" style="zoom:67%;" />

#### 1.1 GAM  

本文中的GAM主要用于accelerator的调度，图中给出了通信过程，core通过GAM来实现accelerator的预约，GAM中记录着 accelerator IDs 的列表和相应 accelerator的 estimated wait times ，在core对某个accelerator预约之后，GAM将request的core的ID写入accelerator的等待列表里面并且按照FCFS执行。

<img src="./7.3.png" alt="7.3" style="zoom:67%;" />

GAM还可以实现accelerator的共享，主要根据以下四个方面：

1) available accelerators的类型; 

2) 每种 accelerator的数量;

3) 在accelerator上运行的jobs的starting times 和 estimated execution times;

4) accelerator的waiting list 以及每个job的estimated runtime 

execution times即accelerator处理时间的回归模型，是用一个基于data-size-parameterized的二次多项式回归模型来预测accelerator时间，如下图所示：

<img src="./7.1.png" alt="7.1" style="zoom:50%;" />

#### 1.2 Accelerator Composition  

accelerator的产生如下图所示，selection criteria是选择的标准。在core调用accelerator之后，会将task description写入shared memory中，可以实现accelerator的组合。（task description：the location of arguments, data layout, which accelerators are involved in the computation, the computation to be performed, and the order in which to perform necessary operations. ）

1）Accelerator Chaining

2)  Accelerator Virtualization

<img src="./7.4.png" alt="7.3" style="zoom:67%;" />

## 2 有价值的idea

主要是提出了ARC的模型，初步提出了GAM的概念和作用，另外还实现了accelerator的组合，可以在本文基础上为GAM多添加一些功能，以及accelerator存取数据的优化，实现一个完整的网络。

## References

[7] J. Cong, M. A. Ghodrat, M. Gill, B. Grigorian and G. Reinman, "Architecture support for accelerator-rich CMPs," DAC Design Automation Conference 2012, San Francisco, CA, 2012, pp. 843-849.



# 8 Learning-Based Application-Agnostic 3D NoC Design for Heterogeneous Manycore Systems

## 1 研究目的

当异构NoC上CPU，GPU，LLC的数目增加的时候，2D的NoC是不能满足需求的，于是提出了3D的NoC，而3D的NoC上的CPU等如何放置是根据latency, throughput, temperature, energy这四个目标来优化决定的。本文基于如何优化提出了MOO-STAGE的多目标优化方法。

#### 1.1 traffic pattern

本文研究了不同的application的traffic pattern，和文献[6]中的一样，根据研究发现，CPU中的通信主要是一个main core的，GPU-GPU的通信也比较少，大部分的是在LLC和GPU之间 many-to-few的通信。而且这种模式和规模以及application是无关的，所以基于这个模式可以提出四种优化目标。

#### 1.2 优化目标

##### 1.2.1 latency

latency就是CPU通信的优化目标，在这里将latency用以下公式来表示：

<img src="./8.1.png" alt="8.1" style="zoom:80%;" />

C代表CPU个数，M代表LLC个数，r是router stages，h是从CPU到LLC的hop个数，d是总体延迟，f是通信次数，优化目标就是使得latency最小。

##### 1.2.2 throughput

throughput就是GPU通信的优化目标，主要是为了实现链路的throughput能够最大，本文认为想要throughput最大，可以转化为每一条链路都被充分利用，就是没有特别拥挤的链路，即利用率：

<img src="./8.2.png" alt="8.2" style="zoom:80%;" />

R 代表tile的个数，p表示core的链路是否是通的，最终是通过U的方差和均值实现：

<img src="./8.3.png" alt="8.3" style="zoom:80%;" />

优化目标就是使得均值和方差最小。

##### 1.2.3  Thermal Requirements

由于3D的NoC集成程度比较高，所以选择合适的排列使得温度降低是有必要的，因此minimize温度T也是优化目标之一。

##### 1.2.4 energy

最后是energy消耗，优化目标是消耗的energy越少越好。

<img src="./8.4.png" alt="8.4" style="zoom:80%;" />

#### 1.3 优化算法

本文提出MOO-STAGE算法，主要分成两个阶段： Local Search和Meta Search，这种算法主要是通过改变训练的初始状态实现全局最优而非局部最优结果。

<img src="./8.5.png" alt="8.5" style="zoom:80%;" />

##### 1.3.1 local search

在local search中，提出了the Pareto hypervolume (PHV)来衡量设计的好坏，目标是maximizePHV，本文算法采用的是 greedy search，当PHV增加的时候，就将这种设计state添加进去，最终返回设计集合，轨迹，最终的设计状态：

 <img src="./8.6.png" alt="8.6" style="zoom:60%;" />

##### 1.3.2 meta search

meta search的目的就是寻找一个evaluate的函数预测从某个初始state开始的结果，从而将一些不太好的starting state丢弃，为local search提供好的starting state。在初始化之后，将local search中的state添加到global state的集合中，当不能再添加的时候，就输出集合。否则，就将之前的轨迹和PHV都添加到训练集合中，并且训练evaluate函数，这里使用 greedy search优化函数，得到restart的design，给local search使用。

<img src="./8.7.png" alt="8.7" style="zoom:60%;" />

使用这种算法可以使得优化时间减少，而且得到的是最优解。



## 2 有用的idea

本文提出了一种3D的NoC架构，并且利用MOO-STAGE的方法进行多目标优化，实验是在GEM5上进行模拟，最终得到一种比较好的排列方式。



## Refercence

[8]Joardar, Biresh Kumar et al. “Learning-Based Application-Agnostic 3D NoC Design for Heterogeneous Manycore Systems.” *IEEE Transactions on Computers* 68 (2019): 852-866.



# 9 Intelligent Hotspot Prediction for Network-on-Chip-Based Multicore Systems

## 1 研究目的

当某个router或者某几个router的收到的packet的速率大于它们可以处理的速率时，就会产生hotspot，而这种hotspot可能会导致整个NoC的瘫痪，本文提出了一种基于ANN的hotspot预测机制，这种方法解决了之前因为application不同而导致预测失败的问题。

#### 1.1 ANN architecture

本文提出的ANN处理模型可以看作是NoC上的一个PE，这个ANN监测附近的router的流量，返回有可能产生hotspot的router的位置。

<img src="./9.1.png" alt="9.1" style="zoom:60%;" />

ANN需要从router获取利用率的信息，检测范围可以是2 × 2，3 × 3，4 × 4，然后将这些组合起来，

<img src="./9.3.png" alt="9.3" style="zoom:60%;" />

ANN的节点表示如下：

<img src="./9.4.png" alt="9.4" style="zoom:60%;" />

输出层输出的是某个路由器是否会出现hotspot，若会，输出1.

当然，范围越小准确性越高，但是当router的范围减小的时候，workload也在增加，所以需要在两者之间找到一种平衡，4 × 4是最优的：

<img src="./9.2.png" alt="9.2" style="zoom:60%;" />

#### 1.2  训练过程

mesh如何分配给ANN如下图所示，重合部分的hotspot的判断可以采用voting scheme，用OR来决定。本文中选用的是8×8的mesh，在此网络中，用4个4×4的ANN，由于没有overlap的router，又设置了一个top-level的ANN，运用OR的方法判定边界上的router是否有可能成为hotspot。本文收集了超过50万个周期的缓冲区利用率数据，对模型进行训练，但是文中只比较了预测结果和真实trace，计算了预测方法的准确率，但是没有提出路由的更改方法。

<img src="./9.5.png" alt="9.2" style="zoom:60%;" />

## 2 有价值的idea

本文提出了一种基于ANN预测NoC中hotspot的方法，可以基于这个方法将预测结果作为反馈用于GEM中负责调解router的分配。

## Refercence

[9] Kakoulli, Elena et al. “Intelligent Hotspot Prediction for Network-on-Chip-Based Multicore Systems.” *IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems* 31 (2012): 418-431.





# 10 BiNoCHS: Bimodal Network-on-Chip for CPU-GPU Heterogeneous Systems

## 1 研究目的

对于GPU和CPU、LLC的异构网络，本文提出了一种可重构的network，在高throughput的时候会激活附加的channel，在低throughput的时候就关闭这些channel。本文主要将工作模式分成两类，CPU-dominated low-traffic 和high-traffic GPU 两种情况，如下图所示，提出了BiNoCHS的model。

<img src="./10.1.png" alt="9.2" style="zoom:80%;" />

### 1.1 BiNoCHS概述

当通信channel增加的时候，power requirement也在增加，为了维持power budget，需要将voltage和frequency降低，然而这样又会增加延迟，因此仅仅增加通信链路是不可行的。

BiNoCHS将通信分成两种，一种是没有增加的通信链路，一种是有，而BiNoCHS通过调节 topology, routing,physical/virtual channel resources,  frequency/voltage来使得两种不同的通信模式都有最高的效率。

### 1.2 优化方法

BiNoCHS提供了两种模式，分别工作在nominal和near-threshold voltage两种模式下，分别对应low-traffic CPU-dominated 和 GPU-heavy NoC-intensive workloads。

#### 1.2.1 More virtual channels

在输入端口将virtual channel的数量增加（比如double），可以减小congestion，但是会给VCA带来负担，需要仲裁的数量增加，因此本文提出了 reconfigurable Reduced Logic Arbiter (RLA)，就是用两个arbiter级联，当在nominal voltage的时候，第二个arbiter就关掉，这样latency就会减小，throughput增大。

<img src="./10.2.png" alt="9.2" style="zoom:80%;" />

#### 1.2.2 Reducing network concentration

network concentration是指每个router连接的节点数量，在throughput较高的时候将network concentration减小，即扩展网络，可以减少拥塞，如下图所示，

<img src="./10.3.png" alt="9.2" style="zoom:60%;" />

这里的扩展方法是将原本的旁路router也投入使用，如图是将B和C以及连接的D加入链路中，

<img src="./10.4.png" alt="9.2" style="zoom:60%;" />

B和C的结构如下图，当nominal voltage下通过红色线传输数据，这个选择由voltage来控制。

<img src="./10_6.png" alt="9.2" style="zoom:80%;" />

#### 1.2.3 Using non-minimal adaptive routing

本文还简要的介绍了adaptive routing的算法，即在threshold-voltage的条件下，允许packet使用不同的通信链路来避免congestion，使得traffic distribution更加均匀，从而增大throughput。在nominal voltage下由于这种算法会增加延迟，因此不使用。



## 2 有价值的idea

本文提出一种双模式工作的方式，从而增大throughput的同时不会增加latency。

## Reference

[10] Mirhosseini, Amirhossein et al. “BiNoCHS: Bimodal network-on-chip for CPU-GPU heterogeneous systems.” 2017 Eleventh IEEE/ACM International Symposium on Networks-on-Chip (NOCS) (2017): 1-8.



# 11 Online Learning for Adaptive Optimization of Heterogeneous SoCs

## 1 研究目的

在异构SoC上，为了实现energy  efficient,本文提出了一种预测 performance, power consumption and temperature 的adaptive model。本文用 GPU frame processing time来衡量performance, GPU power consumption  和power -temperature dynamics衡量后两者。

### 1.1 Adaptive GPU Performance Model 

首先提出了 frame processing time，是GPU frequency和 workload的函数，workload用GPU performance counters来表示，表达式为：

<img src="./11.1.png" alt="9.2" style="zoom:80%;" />

f代表频率，x表示performance counters，

<img src="./11.2.png" alt="9.2" style="zoom:80%;" />

表示了时间与频率及x之间的关系，而

<img src="./11.3.png" alt="9.2" style="zoom:80%;" />

组成了feature set **h**k,然后利用RLS或者LMS迭代，找到使得误差最小的系数，<img src="./11.5.png" alt="9.2" style="zoom:80%;" />

得到预测结果如下：

<img src="./11.15.png" alt="9.2" style="zoom:80%;" />



### 1.2 Adaptive Power Consumption Model 

首先将power consumption写成如下表达式：

<img src="./11.6.png" alt="9.2" style="zoom:80%;" />

其中，leakage电流可以表示为：

<img src="./11.7.png" alt="9.2" style="zoom:80%;" />

其中T表示温度，*c*1 和 *c*2 及***I***gate在offline通过非线性回归得到（另一篇文章），因此下式中的第二项已经得到，

<img src="./11.8.png" alt="9.2" style="zoom:80%;" />

第一项中的主要参数可以表示为workload和frequency的函数，与第一部分中的performance counters一样，运用下图的feature，

<img src="./11.9.png" alt="9.2" style="zoom:80%;" />

可以得到表达式为：

<img src="./11.10.png" alt="9.2" style="zoom:80%;" />

所以运用RLS得到上式，可以预测Power。

### 1.3 Power-Temperature Dynamics

首先动态温度可以表示为：

<img src="./11.11.png" alt="9.2" style="zoom:80%;" />

其中Ct 表示 thermal capacitance, Gt 表示 thermal conductance,T 是 temperature,  P 是power consumption，将时间离散化得到：

<img src="./11.12.png" alt="9.2" style="zoom:80%;" />

A是个矩阵，代表k时刻的T对K+1时刻T的影响，是各个Hotspot处的值，当认为fixed point的状态已经稳定的时候， 可以用下式来预测：

<img src="./11.13.png" alt="9.2" style="zoom:80%;" />

这里的P可以用1.2中的表示：

<img src="./11.14.png" alt="9.2" style="zoom:80%;" />

## 2 有价值的idea

本文提出了一种运用RLS和LMS来预测performance、power consumption和temperature的方式。

## Reference

[11] Bhat, Ganapati et al. “Online Learning for Adaptive Optimization of Heterogeneous SoCs.” *2018 IEEE/ACM International Conference on Computer-Aided Design (ICCAD)* (2018): 1-6.

# 优化图

![脑图](./脑图.png)

# Interconnect

<img src="./7.5.png" alt="7.5" style="zoom:80%;" />