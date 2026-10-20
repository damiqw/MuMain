#pragma once

// Efectos de las mascotas custom de los clientes MuEmu (GetMainInfo), leidos de Data/MuEmu, como
// CustomPetEffect.cpp de Main.dll:
// - CustomPetEffectStatic.txt: sprites sobre los huesos de la mascota (mismo formato que CustomEffectStatic).
// - CustomPetGlow.txt: brillo de color sobre una malla de la mascota (tipo de render 1.04E y color 0 a 255).
// - CustomEarthQuakeSkill.txt: efectos del Earthquake de los caballos custom (onda con color y piedras).
// Las texturas se pasan de los numeros de 1.04E a los de este cliente (MuEmu::Texture).
namespace MuEmu::PetEffect
{
// Dibuja los efectos de la montura `o` si es una mascota custom (despues de dibujar su modelo).
void Render(OBJECT* o);

// Efecto del Earthquake de la montura `o` (en lugar de RenderDarkHorseSkill). Verdadero si es una mascota custom:
// el cliente no dibuja el del Dark Horse.
bool RenderHorseSkill(OBJECT* o, BMD* b);
} // namespace MuEmu::PetEffect
