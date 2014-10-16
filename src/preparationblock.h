/*
 * This file is a part of the Sharemind framework.
 * Copyright (C) Cybernetica AS
 *
 * All rights are reserved. Reproduction in whole or part is prohibited
 * without the written consent of the copyright owner. The usage of this
 * code is subject to the appropriate license agreement.
 */

#ifndef SHAREMIND_LIBVM_PREPARATIONBLOCK_H
#define SHAREMIND_LIBVM_PREPARATIONBLOCK_H

#ifndef SHAREMIND_INTERNAL__
#error including an internal header!
#endif

#include <sharemind/codeblock.h>
#include <sharemind/extern_c.h>


SHAREMIND_EXTERN_C_BEGIN

typedef struct {
    SharemindCodeBlock * block;
    unsigned type;
} SharemindPreparationBlock;

SHAREMIND_EXTERN_C_END

#endif /* SHAREMIND_LIBVM_PREPARATIONBLOCK_H */
