#ifndef PFD_H
#define PFD_H

#include <stdint.h>
#include <TFT_eSPI.h>

#include "SPI.h"


TFT_eSPI TFT480 = TFT_eSPI(); // Configurer le fichier User_Setup.h de la bibliothèque TFT480_eSPI au préalable



/** ***********************************************************************************
	CLASS VOYANT  // sorte de LED qui peut s'allumer de différentes couleurs et afficher un mot				
***************************************************************************************/


class VOYANT
{
public:

	uint16_t x0;
	uint16_t y0;
	uint16_t dx;
	uint16_t dy;

	uint16_t couleur_caract;
	char caract;

	VOYANT();
	
	void init(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy);
	void affiche(uint16_t couleur);
	
private:


};


#endif
