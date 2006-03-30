#include "distmaps.h"
#include "conversions.h"

#include <R_ext/Error.h>
#include <iostream>

using namespace std;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
inline void calc_dist_map(double * data, int& ncol, int& nrow, int& alg);
inline void lz_dist_map(double * data, int& ncol, int& nrow);
inline void os_dist_map(double * data, int& ncol, int& nrow);
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* THIS FUNCTION MODIFIES rimage - COPY BEFORE IF REQUIRED */
SEXP distMap(SEXP rimage, SEXP alg) {
    try {
        int * dim = INTEGER(GET_DIM(rimage));
        int ndim = LENGTH(GET_DIM(rimage));
        int algorithm = INTEGER(alg)[0];
        int ncol = dim[0];
        int nrow = dim[1];
        int nimages = 1;
        if (ndim > 2)
            nimages = dim[2];
        double * data;
        for (int i = 0; i < nimages; i++) {
            data = &(REAL(rimage)[i * ncol * nrow]);
            calc_dist_map(data, ncol, nrow, algorithm);
        }
    }
    catch(...) {
        error("exception within distMap c++ routine");
    }
    return rimage;
}
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
inline void calc_dist_map(double * data, int& ncol, int& nrow, int& alg) {
    switch(alg) {
        case 2: lz_dist_map(data, ncol, nrow); break;
        default: os_dist_map(data, ncol, nrow);
    };
}
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* macros for the use in the function below */
#define LEFTRIGHT() ( \
  { d = sqrt(dx * dx + (y - row) * (y - row)); \
    if (data[index] <= d) { done = true; continue; } \
    if (col - dx >= 0) if (data[col - dx + y * ncol] == 0) done = true; \
    if (col + dx < ncol) if (data[col + dx + y * ncol] == 0) done = true; \
    if (done && d < data[index] ) data[index] = d; \
  } )

#define TOPBOTTOM() ( \
  { d = sqrt(dx * dx + (x - col) * (x - col)); \
    if (data[index] <= d) { done = true; continue; } \
    if (row - dx >= 0) if (data[x + (row - dx) * ncol] == 0) done = true; \
    if (row + dx < nrow) if (data[x + (row + dx) * ncol] == 0) done = true; \
    if (done && d < data[index]) data[index] = d; \
  } )
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* EBImage distance map algorithm - returns real distances: for every pixel that
   is not background an extending square around the pixel is drawn until a background
   point is hit which is closer that the half-side of a square or until the square exceeds the image*/
/* Author: Oleg Sklyar, EBImage, 2006 */
inline void os_dist_map(double * data, int& ncol, int& nrow) {
    /* segmented image is assumed on input. Background must be 0, foreground
       must be 1 */
    int index, x, y, edge1, edge2;
    int maxsize = (ncol > nrow)?ncol:nrow;
    double d;
    bool done, found;
    for (int col = 0; col < ncol; col++)
        for (int row = 0; row < nrow; row++) {
            index = col + row * ncol;
            /* do not do anything with the point if it is background */
            if (data[index] == 0)
                continue;
            else
                data[index] = maxsize + 1;
            found = false;
            /* draw extending square around the point until background is reached or end of image hit */
            for (int dx = 1; dx < maxsize && !found; dx++) {
                /* if the point is already assigned a value that is closer than any in
                   this or next round - stop
                */
                if (data[index] <= dx) {
                    found = true;
                    continue;
                }
                /* check right and left */
                if (col - dx >= 0 || col + dx < ncol) {
                    edge1 = (row - dx >= 0)?(row - dx):0;
                    edge2 = (row + dx < nrow - 1)?(row + dx):(nrow - 1);
                    done = false;
                    for (y = row; y >= edge1 && !done; y--)
                        LEFTRIGHT();
                    done = false;
                    for (y = row + 1; y <= edge2 && !done; y++)
                        LEFTRIGHT();
                }
                /* check top-bottom */
                if (row - dx >= 0 || row + dx < nrow) {
                    edge1 = (col - dx >= 0)?(col - dx):0;
                    edge2 = (col + dx < ncol - 1)?(col + dx):(ncol - 1);
                    done = false;
                    for (x = col; x >= edge1 && !done; x--)
                        TOPBOTTOM();
                    done = false;
                    for (x = col + 1; x <= edge2 && !done; x++)
                        TOPBOTTOM();
                }
            }
        }
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  adapted from Animal Package by Rocardo Fabbri:: distmap-lz.c
  algorithm: R. Lotufo, F. Zampirolli, SIBGRAPI 2001, 100-105, 2001
 - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
inline void lz_dist_map(double * data, int& ncol, int& nrow) {
    /* WH: a very large number - but not so large as that adding more to could lead to
       integer overflow */
    double verylarge = INT_MAX - ncol * ncol - nrow * nrow;

    try {
        /* consider 0 as background and set the rest to large value for binary type */
        /* OS: basically we can accept any image type as input: binary, gray, RGB */
        int i, j;
        for (i = 0; i < ncol * nrow; i++)
           if (data[i] != 0)
               data[i] = verylarge;
        /* STEP 1 vertical */
        /* i are the columns, j are the rows */
        int inc;
        for (i = 0; i < ncol; i++) {
            inc = 1;
            for (j = 1; j < nrow; j++)
                if (MAT_ELT(data, i, j, ncol) > MAT_ELT(data, i, j-1, ncol) + inc) {
                    MAT_ELT(data, i, j, ncol) = MAT_ELT(data, i, j-1, ncol) + inc;
                    inc += 2;
                }
                else
                   inc = 1;
            inc = 1;
            for (j = nrow - 2; j >= 0; j--)
                if (MAT_ELT(data, i, j, ncol) > MAT_ELT(data, i, j+1, ncol) + inc) {
                    MAT_ELT(data, i, j, ncol) = MAT_ELT(data, i, j+1, ncol) + inc;
                    inc += 2;
                }
                else
                    inc = 1;
        }
        /* STEP 2 horizontal */
        int * wq  = (int *) R_alloc(ncol, sizeof(int));
        int * wq2 = (int *) R_alloc(ncol, sizeof(int));
        int * eq  = (int *) R_alloc(ncol, sizeof(int));
        int * eq2 = (int *) R_alloc(ncol, sizeof(int));
        int wqEnd, wqIni, eqEnd, eqIni, wq2End, eq2End;
        int * tmpq;
        double * point;
        double * ptr = data;
        for (j = 0; j < nrow; j++) {
            for (i = 0; i < ncol - 1; ++i) {
                wq[i] = i + 1;
                eq[i] = ncol - i - 2;
            }
            wqEnd = eqEnd = ncol - 1;
            wqIni = eqIni = 0;
            inc = 1;
            /* while queue is not empty */
            while (wqEnd > wqIni || eqEnd > eqIni) {
                wq2End = eq2End = 0;
                while (eqEnd > eqIni) {
                    i = eq[eqIni++];   /* remove from queue */
                    point = ptr + i;
                    if (point[1] > *point + inc) {
                        point[1] = *point + inc;
                        if (i + 1 < ncol - 1)
                            eq2[eq2End++] = i + 1; /* insert into queue */
                    }
                }
                while (wqEnd > wqIni) {
                    i = wq[wqIni++];
                    point = ptr + i - 1;
                    if (*point > point[1] + inc) {
                        *point = point[1] + inc;
                        if (i - 1 > 0)
                            wq2[wq2End++] = i - 1;
                    }
                }
                tmpq = wq; wq = wq2; wq2 = tmpq;
                tmpq = eq; eq = eq2; eq2 = tmpq;
                wqEnd = wq2End;
                eqEnd = eq2End;
                wqIni = 0;
                eqIni = 0;
                inc += 1; // TODO 2;
            }
            ptr += ncol;
        }
    }
    catch(...) {
      error("memory problems in 'lz_dist_map' c++ routine");
    }
}
