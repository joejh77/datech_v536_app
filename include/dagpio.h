#ifndef _DAGPIO_H
#define _DAGPIO_H

#ifdef __cplusplus
extern "C" {
#endif

int dagpio_pin_read(int pin);
int dagpio_main(int argc, char** argv);

#ifdef __cplusplus
}
#endif
#endif //_DAGPIO_H