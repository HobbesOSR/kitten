CPPFLAGS += -Ilinux/ofed/$(CONFIG_OFED)/include

ifndef CONFIG_OFED
	CONFIG_OFED = 1.5.1
endif 

obj-$(CONFIG_INFINIBAND)	+= $(CONFIG_OFED)/drivers/infiniband/core/
obj-$(CONFIG_INFINIBAND_MTHCA)          += $(CONFIG_OFED)/drivers/infiniband/hw/mthca/
obj-$(CONFIG_MLX4_INFINIBAND)           += $(CONFIG_OFED)/drivers/infiniband/hw/mlx4/
obj-$(CONFIG_MLX4_CORE) += $(CONFIG_OFED)/drivers/net/mlx4/
obj-$(CONFIG_INFINIBAND_QIB)           += $(CONFIG_OFED)/drivers/infiniband/hw/qib/
