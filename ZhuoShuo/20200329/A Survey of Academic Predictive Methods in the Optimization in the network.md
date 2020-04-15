# A Survey of Academic Predictive Methods in the Optimization in the Network

### 1 Introduction

在多核处理器的系统中如果可以对某项参数进行预估，这样可以对以后的行为做出预判，从而选择最佳的方案，节省时间，节省能量消耗，提高效率。本文主要调研了机器学习在系统中的应用，[1]中比较系统的介绍了各种预测方法的应用，如fig.1所示，除此之外本文分析了强化学习中的Q-learning的应用。

<img src="./fig1.png" alt="fig1" style="zoom: 50%;" />

### 2 Prediction and Classification Techniques  

#### 2.1  Basic Prediction Techniques

[1]中首先介绍了三种基本的预测方案：

1）History Predictor

这种方法主要优点是比较简单，易于在硬件上实现，可以用来预测拥堵的发生，[2]，[3]中提出这样可以适时更改路由器的工作频率，减少功率消耗。从而实现 dynamic voltage and frequency scaling (DVFS)，这样可以通过损失最少的性能来减少能量消耗。

2）Autoregressive Moving Average (ARMA) Model

ARMA模型的建立分为两个阶段:识别估计阶段和模型检验阶段。[4]，[5]中的ARMA模型主要应用在SoC的thermal management，预测热因子。

3）*Kalman Filters

一种用于预测离散控制过程状态x的自适应滤波器,它使用一组递归方程，并采用反馈控制机制，使估计误差的方差最小化。[6]中提出了一种运用Kalman Filters预测工作负载处理时间的方法。

#### 2.2 Advanced Prediction Via Classification Techniques

1) Linear Regression (LR)

[7]中将线性回归运用到thermal预测中，[8]用来实现DVFS的调节。

2) *Support Vector Machines (SVMs)

[9]将SVMs运用到异构平台的多任务分配中，[10]运用SVM来预测NoC中的时间延迟。

3）Reinforcement Learning (RL)

[11]将RL运用在thermal management中，[12]将RL运用在online power management中，[13]主要是运用在DVFS中。

4）Neural Network (NN) Models

[14]用于在NoC上面预测拥塞的hotspot，主要是通过ANN的方法，基于synthetic traffic traces进行训练，从而对traffic traces进行预测，来避免出现hotspot的情况。

[15]利用神经网络模型对节点和链路的功率和热分布进行了估计。



### 3 Q-learning

Q-learning主要是根据reward的大小来确定action，主要有environment、agent、state、action和reward几个因素，而最初的表达式如下所示，可以将这个运用到数据传输节省时间，

<img src="./fig2" alt="fig2" style="zoom: 50%;" />

[16]提出了一种基于Q-learning的容错控制机制，[17]将RL与ANN结合，在实现online prediction的同时缩小Q-table。

### References

[1]C. Ababei and M. G. Moghaddam, "A Survey of Prediction and Classification Techniques in Multicore Processor Systems," in IEEE Transactions on Parallel and Distributed Systems, vol. 30, no. 5, pp. 1184-1200, 1 May 2019.

[2]L. Shang, L.-S. Peh, and N. K. Jha, “Dynamic voltage scaling with links for power optimization of interconnection networks,” in Proc. 9th Int. Symp. Int. Symp. High-Perform. Comput. Archit., 2003,pp. 91–102.

[3] C. Ababei and N. Mastronarde, “Benefits and costs of prediction based DVFS for NoCs at router level,” in Proc. 27th IEEE Int. System-on-Chip Conf., 2014, pp. 255–260.

[4] A. K. Coskun, T. S. Rosing, and K. Gross, “Utilizing predictors for efficient thermal management in multiprocessor SoCs,” IEEE Trans. Comput.-Aided Des. Integr. Circuits Syst., vol. 28, no. 10,pp. 1503–1516, Oct. 2009.

[5] G. Bhat, G. Singla, A. K. Unver, and U. Y. Ogras, “Algorithmic optimization of thermal and power management for heterogeneous mobile platforms,” IEEE Trans. Very Large Scale Integr.Syst., vol. 26, no. 3, pp. 544–557, Mar. 2018.

[6] S.-Y. Bang, K. Bang, S. Yoon, and E.-Y. Chung, “Run-time adaptive workload estimation for dynamic voltage scaling,” IEEE Trans. Comput.-Aided Des. Integr. Circuits Syst., vol. 28, no. 9,pp. 1334–1347, Aug. 2009.

[7]  I. Yeo, C. C. Liu, and E. J. Kim, “Predictive dynamic thermal management for multicore systems,” ACM/IEEE Des. Autom. Conf., 2008, pp. 734–739.

[8]   E. Cai, D. C. Juan, S. Garg, J. Park, and D. Marculescu, “Learning based power/performance optimization for many-core systems with extended-range voltage/frequency scaling,” IEEE Trans. Comput.-Aided Des. Integr. Circuits Syst., vol. 35, no. 8, pp. 1318–1331, Aug. 2016.

[9] Y. Wen, Z. Wang, and M. F. P. O’Boyle, “Smart multi-task scheduling for OpenCL programs on CPU/GPU heterogeneous platforms,” in Proc. 21st Int. Conf. High Perform. Comput.,pp. 1–10, 2014.

[10] Z. Qian, D.-C. Juan, P. Bogdan, C.-Y. Tsui, D. Marculescu, and R. Marculescu, “SVR-NoC: A performance analysis tool for network-on-chips using learning-based support vector regression model,” in Proc. Des. Autom. Test Eur. Conf. Exhib., 2013, pp. 354–357.
[11] A. Das, R. A. Shafik, G. V. Merrett, B. M. Al-Hashimi, A. Kumar, and B. Veeravalli, “Reinforcement learning-based inter- and intra-application thermal optimization for lifetime improvement of multicore systems,” in Proc. 51st ACM/EDAC/IEEE Des. Autom.
Conf., 2014, pp. 1–6.
[12] H. Shen, Y. Tan, J. Lu, Q. Wu, and Q. Qiu, “Achieving autonomous power management using reinforcement learning,” ACM Trans. Des. Autom. Electron. Syst., vol. 18, 2013, Art. no. 24.
[13] Y.-G. Chen, W.-Y. Wen, T. Wang, Y. Shi, and S.-C. Chang, “Q-Learning based dynamic voltage scaling for designs with graceful degradation,” in Proc. Symp. Int. Symp. Phys. Des., 2015,pp. 41–48.

[14]  E. Kakoulli, V. Soteriou, and T. Theocharides, “Intelligent hotspot prediction for network-on-chip-based multicore systems,” IEEE Trans. Comput.-Aided Des. Integr. Circuits Syst., vol. 31, no. 3,pp. 418–431, Mar. 2012

[15] M. F. Reza, T. T. Le, B. De, M. Bayoumi, and D. Zhao, “NeuroNoC: Energy optimization in heterogeneous many-core NoC using neural networks in dark silicon era,” in Proc. IEEE Int. Symp. Circuits Syst., 2018, pp. 1–5.

[16]K. Wang, A. Louri, A. Karanth and R. Bunescu, "High-performance, energy-efficient, fault-tolerant network-on-chip design using reinforcement learning," 2019 Design, Automation & Test in Europe Conference & Exhibition (DATE), Florence, Italy, 2019, pp. 1166-1171.

[17]H. Zheng and A. Louri, "An Energy-Efficient Network-on-Chip Design using Reinforcement Learning," 2019 56th ACM/IEEE Design Automation Conference (DAC), Las Vegas, NV, USA, 2019, pp. 1-6.