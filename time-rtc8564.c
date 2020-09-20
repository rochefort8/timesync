

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <string.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "rtc8564.h"

#define I2C_DEVICE_PATH "/dev/i2c-1"

static int file = -1 ;

static inline int htod(int hex) {
  return (((hex / 10) << 4) | (hex % 10));
}
 
static inline int dtoh(int dec) {
  return ((dec >> 4) * 10 + (dec & 0x0f));
}

static inline __s32 i2c_smbus_access(int file, char read_write, __u8 command,
                                     int size, union i2c_smbus_data *data)
{
	struct i2c_smbus_ioctl_data args;

	args.read_write = read_write;
	args.command = command;
	args.size = size;
	args.data = data;
	return ioctl(file,I2C_SMBUS,&args);
}

static inline __s32 i2c_smbus_read_byte_data(int file, __u8 command,__u8 *value)
{
    union i2c_smbus_data data;
    int err;

    err = i2c_smbus_access(file, I2C_SMBUS_READ, command,
                I2C_SMBUS_BYTE_DATA, &data);
    if (err < 0) {
        return err;
    }

    *value = (0x0FF & data.byte) ;
    return 0 ;
 }
  
static inline __s32 i2c_smbus_write_byte_data(int file, __u8 command, __u8 value)
{
    union i2c_smbus_data data;
    data.byte = value;
    return i2c_smbus_access(file, I2C_SMBUS_WRITE, command,
                I2C_SMBUS_BYTE_DATA, &data);
}
  
int read_byte_register(uint8_t reg, uint8_t *val)
{
    return i2c_smbus_read_byte_data(file,reg,val);
}

int write_byte_register(uint8_t reg, uint16_t val)
{
    return i2c_smbus_write_byte_data(file,reg,val);
}

int rtc8564_open(void)
{
    char *path = I2C_DEVICE_PATH ;
    uint8_t address = RTC8564_I2C_ADDRESS ;

	file = open(path, O_RDWR);
	if (file < 0) {
		fprintf(stderr,"Failed to open %s, errno=%d", path, errno) ; 
        return -1 ;
    }
	int ret = ioctl(file, I2C_SLAVE, address);
	if (ret < 0) {
        return ret ;
    }
    return 0 ;
}

static int rtc8564_close(void)
{
    close(file) ;
    file = -1 ;
    return 0 ;
}

static int rtc8564_init_registers(void)
{
    write_byte_register( RTC8564_CONTROL1 , RTC8564_STOP_BIT );
    write_byte_register( RTC8564_CONTROL2 , 0 );
    write_byte_register( RTC8564_CLKOUT_FREQUENCY , RTC8564_CLKOUT_32768HZ ) ;

    write_byte_register( RTC8564_MINUTE_ALARM,  RTC8564_AE_NONE ) ;
    write_byte_register( RTC8564_HOUR_ALARM,    RTC8564_AE_NONE ) ;
    write_byte_register( RTC8564_DAY_ALARM,     RTC8564_AE_NONE ) ;
    write_byte_register( RTC8564_WEEKDAY_ALARM, RTC8564_AE_NONE ) ;
}

static int rtc8564_get_time(struct tm *timeptr)
{
    uint8_t data[7] ;
    int i ;

    write_byte_register( RTC8564_CONTROL1 , RTC8564_STOP_BIT );
    for (i = 0;i < 7;i++) {
        int ret = read_byte_register( RTC8564_SECONDS + i, &data[i] ) ; 
        if (ret != 0) {
            write_byte_register( RTC8564_CONTROL1 , 0 );
            return -1 ;
        }
    }
    write_byte_register( RTC8564_CONTROL1 , 0 );

    // TODO
    // valodation of time info

    timeptr->tm_sec     = dtoh ((int)data[0] & 0x7f ) ;
    timeptr->tm_min     = dtoh ((int)data[1] & 0x7f ) ;
    timeptr->tm_hour    = dtoh ((int)data[2] & 0x3f ) ;
    timeptr->tm_mday    = dtoh ((int)data[3] & 0x3f) ;
    timeptr->tm_wday    = dtoh ((int)data[4] & 0x07 ) ;
    timeptr->tm_mon     = dtoh ((int)data[5] & 0x1f ) - 1 ;
    timeptr->tm_year    = dtoh ((int)data[6]) + 100 ;
    return 0 ;
}

static int rtc8564_set_time(struct tm *timeptr)
{
    int ret = 0 ;
    write_byte_register( RTC8564_CONTROL1 , RTC8564_STOP_BIT );

    write_byte_register( RTC8564_SECONDS,       (uint8_t)htod( timeptr->tm_sec)) ; 
    write_byte_register( RTC8564_MINUTES,       (uint8_t)htod( timeptr->tm_min)) ; 
    write_byte_register( RTC8564_HOURS,         (uint8_t)htod( timeptr->tm_hour)) ; 
    write_byte_register( RTC8564_DAYS,          (uint8_t)htod( timeptr->tm_mday)) ; 
    write_byte_register( RTC8564_WEEKDAYS,      (uint8_t)htod( timeptr->tm_wday)) ;    
    write_byte_register( RTC8564_MONTH_CENTURY, (uint8_t)htod( timeptr->tm_mon + 1 )) ;     
    write_byte_register( RTC8564_YEARS,         (uint8_t)htod( timeptr->tm_year % 100)) ;     

    write_byte_register( RTC8564_CONTROL1 , 0 );

    return ret ;
}

static inline bool is_valid_decimal(int dec) 
{
    return ( ((dec >> 4) < 10) && ((dec & 0x0f) < 10) ) ;
}

static int rtc8564_read_registers(void)
{
    uint8_t offset;

    for (offset = 0;offset < 0x10; offset++) {
        uint8_t val ;
        read_byte_register(offset,&val) ;
        printf("%02x : %02x\n", offset, val) ;
    }
}

static int system_get_time(struct tm *timeptr)
{
    struct timespec ts ;

    int ret = clock_gettime(CLOCK_REALTIME, &ts);
    if (ret != 0 ) {
        return ret ;
    }

	localtime_r( &ts.tv_sec, timeptr); 
    return 0 ; 
}

static int system_set_time(struct tm *timeptr)
{
    struct timespec ts ;
    int ret ;

//    printf(asctime(timeptr));

    ts.tv_sec   = mktime(timeptr) ;
    ts.tv_nsec  = 0 ;

    ret = clock_settime(CLOCK_REALTIME, &ts);
    if (ret != 0) {
        printf("Could not set time. errno=%d\n",errno) ;
        return -1 ;
    }
    return 0 ;
}

static int rtc8564_is_power_on_reset(bool *yes_no) 
{
    uint8_t data ;

    int ret = read_byte_register( RTC8564_SECONDS, &data ) ; 
    if (ret != 0) {
        return ret ;
    }
    *yes_no = (data & RTCS8564_CAL_VL)? true : false ;
    return 0 ;
}

int rtc8564_do_set_current_systen_time(void)
{
    struct tm now ;
    int ret ;
    
    ret = system_get_time(&now) ;
    if (ret != 0) {
        return ret ;
    }
    ret = rtc8564_set_time(&now) ;
    if (ret != 0) {
        return ret ;
    }
    return 0 ;
}

int rtc8564_do_timesync(void)
{
    int ret ;
 
    ret = rtc8564_open() ;
    if (ret != 0) {
        return ret ;
    }

    bool is_por ;
    ret = rtc8564_is_power_on_reset(&is_por) ;
    if (ret != 0) {
        goto end ;
    }

    if (is_por == true) {
        printf("Power on Reset\n");

        ret = rtc8564_init_registers() ;
        if (ret != 0) {
            goto end ;
        }
        struct tm now_system ;

        ret = system_get_time(&now_system) ;
        if (ret != 0) {
            goto end ;
        }        
        printf("Set RTC time to %s\n",asctime(&now_system));
        ret = rtc8564_set_time(&now_system) ;
    } else {
        struct tm now_system, now_rtc ;

        ret = system_get_time(&now_system) ;
        if (ret != 0) {
            goto end ;
        }        

        ret = rtc8564_get_time(&now_rtc) ;
        if (ret != 0) {
            goto end ;
        }
        
        printf("RTC time : %s", asctime(&now_rtc));
        printf("SYS time : %s", asctime(&now_system));
        double delta = difftime(mktime(&now_rtc), mktime(&now_system)) ;
        printf("Diff=%f\n",delta) ;
        if (delta > 0) {            
            system_set_time(&now_rtc);
        }
    }

end : 
    rtc8564_close();
    return ret ;
}

int main(int argc, char *argv[]) 
{
    int ret ;

    ret = rtc8564_do_timesync() ;
    if (ret != 0) {
        printf("Error\n");
     }
     return 0 ;
}

