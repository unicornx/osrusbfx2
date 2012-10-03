//-----------------------------------------------------------------------------
//   File:      periph.h
//   Contents:  Peripheral related function prototypes. 
//              Specific for CY001 dev board only
// $Archive:  $
// $Date:  $
// $ $
//
//   Copyright (c) , All rights reserved
//-----------------------------------------------------------------------------
#ifndef PERIPH_H // Header entry
#define PERIPH_H

// Bar Graph
void BG_Init ( void );
void BG_Set  ( void );
void BG_Get  ( void );

// Mouse Position tracking simulation
void MM_Init ( void );

#endif // PERIPH_H
