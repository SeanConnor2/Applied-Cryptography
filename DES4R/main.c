//
//  main.c
//  Proj01_DES4r
//
//  Created by Xiaowen Zhang on 9/30/18.
//  Copyright © 2018 __CUNY_CSI__. All rights reserved.
//

#include "des4r.h"
#include <stdio.h>




int main(int argc, const char * argv[]) {
    // insert code here...
	mbedtls_des_self_test( 1);
    return 0;
}
