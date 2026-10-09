/* soc_sim.h —— 只有主机版才编的头文件：虚拟硬件对外提供的两个接口 */
#ifndef SOC_SIM_H
#define SOC_SIM_H

/* 把外设寄存器区真的映射成一段内存，然后准备好所有"外设反应" */
void soc_sim_init(void);

#endif /* SOC_SIM_H */
