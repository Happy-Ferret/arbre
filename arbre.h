/*
 * arbre
 *
 * (c) 2011 Alexis Sellier.
 *
 * arbre.h
 *
 *   common header file for many modules
 *
 */
#include  <stdbool.h>
#include  <stdint.h>
#include  <stdarg.h>

#include  "source.h"
#include  "tokens.h"
#include  "token.h"
#include  "value.h"
#include  "symtab.h"
#include  "node.h"
#include  "tree.h"
#include  "io.h"

#define  ARBRE_DIR      ".arbre"
#define  ARBRE_BIN_DIR  ".arbre/bin"
#define  ARBRE_BIN_FMT  ".arbre/bin/%s.bin"
