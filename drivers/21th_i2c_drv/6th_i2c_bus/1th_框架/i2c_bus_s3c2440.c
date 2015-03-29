

static int s3c2440_i2c_xfer(struct i2c_adapter *adap,
			struct i2c_msg *msgs, int num)
{
	
}


/* declare our i2c functionality */
static u32 s3c2440_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_PROTOCOL_MANGLING;
}


static const struct i2c_algorithm s3c2440_i2c_algo = {
//	.smbuf_xfer			= ,
	.master_xfer		= s3c2440_i2c_xfer,
	.functionality		= s3c2440_i2c_func,
};


/* 1. ∑÷≈‰/…Ë÷√adapter */
static struct i2c_adapter s3c2440_i2c_adapter = {
	.owner		= THIS_MODULE,
	.name       = "s3c2440_i2c",
	.algo		= &s3c2440_i2c_algo,
};


static int i2c_bus_s3c2440_init(void)
{
	/* 2.  ◊¢≤·i2c_adapter */

	i2c_add_adapter(&s3c2440_i2c_adapter);
	return 0;
}

static void i2c_bus_s3c2440_exit(void)
{
	i2c_del_adapter(&s3c2440_i2c_adapter);
}


module_init(i2c_bus_s3c2440_init);
module_exit(i2c_bus_s3c2440_exit);

MODULE_LICENSE("GPL");


