# A Survey of the Interconnection Network for Accelerator-rich Architectures

### 1 Introduction

与通用的processor相比，accelerator可以提高几个数量级的处理速度，CPU对于时间延迟更敏感，而GPU之类的accelerator的throughput会受到accelerator与内存系统数据传输速率的限制，所以在异构的多核系统中，很有必要合理的设计interconnect network，使得CPU的时间延时小的同时，accelerator与内存系统之间数据传输延时也最小。

本文主要将之前的设计interconnect network的论文进行分类、总结分析。

### 2 Basic Interconnect

通常，通用processor或者主processor是由core和main memory组成，而accelerator由core、local memory组成，如何连接core和memory使得延时最小是本文研究的主要问题。

在论文[4]中提出了四种基本的interconnect，分别是：

1）Bus-based interconnect
2）NoC-based interconnect
3）Shared memory
4）Crossbar

主要是从scatability，system performance，area-efficiency三个方面衡量这些网络的性能。fig.1中，Bus-based interconnect空间利用率最高，NoC虽然scatability和performance都很好，但是area-efficiency不高，所以本文接下来主要总结了如何混合这些interconnect，使得性能达到最好。

<center>
    <img style="border-radius: 0.3125em;
    box-shadow: 0 2px 4px 0 rgba(34,36,38,.12),0 2px 10px 0 rgba(34,36,38,.08);" 
    src="./fig1.png" width = "45%" alt=""/>
    <br>
    <div style="color:orange; border-bottom: 1px solid #d9d9d9;
    display: inline-block;
    color: #999;
    padding: 2px;">
      fig.1
  	</div>
</center>

### 3 Advanced Interconnect

#### 3.1 hybrid interconnect based on bus

最基本的Bus的interconnect如fig.2所示，图中假设两个kernel之间的数据传输是必须经过host processor，这样会消耗大量的时间，因此[4]将Bus与其他的结构结合：

<center>
    <img style="border-radius: 0.3125em;
    box-shadow: 0 2px 4px 0 rgba(34,36,38,.12),0 2px 10px 0 rgba(34,36,38,.08);" 
    src="./fig2.png" width = "40%" alt=""/>
    <br>
    <div style="color:orange; border-bottom: 1px solid #d9d9d9;
    display: inline-block;
    color: #999;
    padding: 2px;">
      fig.2
  	</div>
</center>

1）与DMA混合：两个kernel的local memory之间的数据传输使用DMA进行存取，节省了从Host processor传输的时间；

2）与crossbar混合：两个kernel的local memory之间的数据传输使用crossbar进行存取；

3）与DMA和crossbar同时混合：两个kernel的local memory之间的数据传输使用crossbar进行存取，其他的kernel可以使用DMA进行数据传输；

#### 3.2 interconnect based on shared-memory

基于shared-memory的interconnect的performance和空间利用率都很高，这种方法就是将数据存放在shared-memory中，这样可以减少从main-memory存取数据的时间。在accelerator和shared-memory之间用crossbar连接，设计连接方法使得switch的数量可以达到最少的同时interconnect的路由性能可以达到最优。根据accelerator是否具有相同的data port可以分成两种：

##### 3.2.1 the number of data port is uniform

当每个accelerator的data port都相等时，[1]借助以下三个特点优化连接，最终使得switch的数量在不影响路由性能的条件下达到最少：

1）相同加速器的多个数据端口一起通电/断开； 

2）芯片可以同时power的加速器个数是有限的；

3）异构的加速器同时被power的可能性小于同构的acceleraor.

##### 3.2.2 the number of data port is different

当每个accelerator的data port不相等时，[2]提出了memory bank对应accelerator的各个data port的分配算法,假设异构accelerator共有n个，最多能够power的数量为c个，首先对于accelerator按照data port数量d进行从大到小排序，然后将最多容纳的c个accumulator和memory bank进行一对一分配，并分成了c个region。然后将剩下的n-c个accelerator进行一对c分配，即在每个region内重新一轮分配，使得switch的数量在路由性能最优的情况下最小。

#### 3.3 hybrid interconnect based on NoC

基于NoC的interconnect的基本思想是将不同processor的core和memory分别连接到NoC上，但是NoC的area-efficiency很低，因为每一个网络的代价很大，[4]提出了将NoC与基于shared-memory结合，并设计了如何根据accelerator功能选择interconnect的算法。在NoC上传输的数据由于到达memory的时间是无序的，所以对于一个memory的数据存取会因为顺序问题产生延迟，[5]提出了一种基于token的network，对发向memory的request进行排序，从而使得memory的存取时间减少。[3]和[5]提出了一种将circuit-switching和packet-switching结合，利用hybrid-network传输数据的方法。

##### 3.3.1 hybrid-network based on time slot  

[5]将时间分成不同的时间slot，采用TDM的方式进行数据传输，针对CPU一般会选择packet-switching，而GPU之类的accelerator选用hybrid -switching,如fig.3所示：

<center>
    <img style="border-radius: 0.3125em;
    box-shadow: 0 2px 4px 0 rgba(34,36,38,.12),0 2px 10px 0 rgba(34,36,38,.08);" 
    src="./fig3.png" width = "45%" alt=""/>
    <br>
    <div style="color:orange; border-bottom: 1px solid #d9d9d9;
    display: inline-block;
    color: #999;
    padding: 2px;">
      fig.3
  	</div>
</center>

同时文章还提出基于circuit的两种链路共享方式：Hitchhiker-Sharing和Vicinity-Sharing，共享链路资源，节省链路搭建时间。

##### 3.3.2 hybrid-network based on GEM

[3]同样采用的是circuit-switching和packet-switching结合，但是与[5]不同的是，[3]提出了GEM管理的方法来选择数据传输的方式：文章假设有一个 global accelerator manager来协助进行动态负载平衡和任务调度，可以将任务按照预测传输时间进行排序，并对任务列表中的任务分配不同的router和buffer，并判断是选择circuit-switching还是packet-switching。

### 4 Other optimization method 

除了上述方法，[4]提出了在设计interconnect的时候，应该是选择哪种interconnect的算法。文章首先提出了四种solution，然后根据QUAD toolset（即detailed profile），选择不同的解决方案。

### References

[1] J. Cong and B. Xiao, "Optimization of interconnects between accelerators and shared memories in dark silicon," 2013 IEEE/ACM International Conference on Computer-Aided Design (ICCAD), San Jose, CA, 2013, pp. 630-637.

[2]Y. Chen and J. Cong, "Interconnect synthesis of heterogeneous accelerators in a shared memory architecture," 2015 IEEE/ACM International Symposium on Low Power Electronics and Design (ISLPED), Rome, 2015, pp. 359-364.

[3]J. Cong, M. Gill, Y. Hao, G. Reinman and Bo Yuan, "On-chip interconnection network for accelerator-rich architectures," 2015 52nd ACM/EDAC/IEEE Design Automation Conference (DAC), San Francisco, CA, 2015, pp. 1-6

[4]Pham-Quoc, Cuong. (2015). Hybrid Interconnect Design for Heterogeneous Hardware Accelerators. 10.4233/uuid:862e5b58-b9d1-462a-90b0-6f94a054632e. 

 [5]Yin, J. . (2015). Time-division-multiplexing based hybrid-switched noc for heterogeneous multicore systems. *Dissertations & Theses - Gradworks*.

