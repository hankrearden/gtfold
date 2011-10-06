/**
GTfold: compute minimum free energy of RNA secondary structure
Copyright (C) 2008  David A. Bader
http://www.cc.gatech.edu/~bader

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

@author prashant {pgaurav@gatech.edu}

 */

#include <cstdio>

#include "constants.h"
#include "energy.h"
#include "utils.h"
#include "global.h"
#include "subopt_traceback.h"

//#define DEBUG 1

const char* lstr[] = {"W", "V", "VBI", "VM", "WM", "WMPrime"};

void (*trace_func[6]) (int i, int j, ps_t& ps, ps_stack_t& gs);
static int delta = 0;
static int mfe = INFINITY_;
static int length = -1;
static int gflag = 0;

void process(ss_map_t& subopt_data, int len) {
        int count = 0;
        ps_stack_t gstack;


        // initialize the partial structure, segment stack = {[1,n]}, label = W, list_bp = {} 
        ps_t first(0, len);
        first.push(segment(1, len, lW, W[len]));	
        gstack.push(first); // initialize the partial structure stacka

        while (1) {
                if (gstack.empty()) break; // exit
                ps_t ps = gstack.top();
                gstack.pop();

                if (ps.empty()) {
                        count++;
                        subopt_data.insert(std::make_pair<std::string,int>(ps.str,ps.ae_));
                        continue;
                }	
                else {
                       segment smt = ps.top();
                        ps.pop();

                        gflag = 0;
                        if (smt.j_ - smt.i_ > TURN) {
                                (*trace_func[smt.label_])(smt.i_, smt.j_, ps, gstack);
                        }

                        // discarded current segment, using remaining ones
                        if (!gflag) {
                                ps_t ps1(ps);
                                gstack.push(ps1);
                        }
                }
        }

#ifdef DEBUG 
        printf("# SS = %d\n", count);
#endif
}

ss_map_t subopt_traceback(int len, int _delta) {
        trace_func[0] = traceW;
        trace_func[1] = traceV;
        trace_func[2] = traceVBI;
        trace_func[3] = traceVM;
        trace_func[4] = traceWM;
        trace_func[5] = traceWMPrime;

        mfe = W[len];
        delta = _delta;
        length = len;

        ss_map_t subopt_data;
        process(subopt_data, len);

        return subopt_data;
}

void traceV(int i, int j, ps_t& ps, ps_stack_t& gstack) {
        // Hairpin Loop
        if (eH(i,j) + ps.total()  <= mfe + delta) {
                ps_t ps1(ps); 
                ps1.accumulate(eH(i,j));
                ps1.update(i, j, '(', ')');
                push_to_gstack(gstack, ps1);
        }

        // Stack
        if (eS(i, j) + V(i+1, j-1) + ps.total() <= mfe + delta) {
                ps_t ps1(ps);
                ps1.push(segment(i+1, j-1, lV, V(i+1, j-1)));
                ps1.accumulate(eS(i,j));
                ps1.update(i, j , '(', ')');
                push_to_gstack(gstack, ps1);
        }

        // Internal Loop
        if (VBI(i,j) + ps.total() <= mfe + delta) {
                traceVBI(i,j,ps,gstack);
        }

        // Multiloop
        if ( VM(i,j) + ps.total() <= mfe + delta) {
                ps_t ps1(ps);
                ps1.push(segment(i, j, lVM, VM(i,j)));
                ps1.update(i, j, '(', ')');
                push_to_gstack(gstack, ps1);
        }
}

void traceVBI(int i, int j, ps_t& ps, ps_stack_t& gstack) {
        int p,q;

        for (p = i+1; p <= MIN(j-2-TURN,i+MAXLOOP+1) ; p++) {
                int minq = j-i+p-MAXLOOP-2;
                if (minq < p+1+TURN) minq = p+1+TURN;
                int maxq = (p==(i+1))?(j-2):(j-1);
                for (q = minq; q <= maxq; q++) {
                        if (V(p, q) + eL(i, j, p, q) + ps.total() <= mfe + delta) {
                                ps_t ps1(ps);
                                ps1.push(segment(p, q, lV, V(p, q)));
                                ps1.update(i, j , '(', ')');
                                ps1.accumulate(eL(i, j, p, q));
                                push_to_gstack(gstack, ps1);
                        }
                }
        }
}

void traceW(int i, int j, ps_t& ps, ps_stack_t& gstack) {

        for (int l = i; l < j-TURN; ++l) {
                int wim1 =  MIN(0, W[l-1]);
                int d3 = (l>i)?Ed3(j,l,l-1):0;
                int d5 = (j<length)?Ed5(j,l,j+1):0;

                int Wij = V(l,j) + auPenalty(l, j) + d3 + d5 + wim1;
                if (Wij + ps.total() <= mfe + delta ) {
                        ps_t ps1(ps);
                        ps1.push(segment(l, j, lV, V(l,j)));
                        if (wim1 == W[l-1]) ps1.push(segment(i, l-1, lW, W[l-1]));
                        ps1.accumulate(auPenalty(l, j) + d3 + d5);
                        push_to_gstack(gstack, ps1);
                }
        }

        if (W[j-1] + ps.total() <= mfe + delta) {
                ps_t ps1(ps);
                ps1.push(segment(i, j-1, lW, W[j-1]));
                push_to_gstack(gstack, ps1);
        }
}

void traceWM(int i, int j, ps_t& ps, ps_stack_t& gstack) {
        int d3 = (i==1)?Ed3(j,i,length):Ed3(j,i,i-1);
        int d5 = Ed5(j,i,j+1);

        if (V(i,j) + auPenalty(i, j) + Eb + d3 + d5 + ps.total() <= mfe + delta) {
                ps_t ps_new(ps);
                ps_new.accumulate(auPenalty(i, j) + Eb + d3 + d5);
                ps_new.push(segment(i,j, lV, V(i,j)));
                push_to_gstack(gstack, ps_new);
        }

        if (WMPrime[i][j] + ps.total() <= mfe + delta) {
                ps_t ps_new(ps);
                ps_new.push(segment(i,j, lWMPrime, WMPrime[i][j]));
                push_to_gstack(gstack, ps_new);
        }

        if (WM(i+1,j) + Ec + ps.total() <= mfe + delta) {
                ps_t ps_new(ps);
                ps_new.accumulate(Ec);
                ps_new.push(segment(i+1,j, lWM, WM(i+1,j)));
                push_to_gstack(gstack, ps_new);
        }

        if (WM(i,j-1) + Ec + ps.total() <= mfe + delta) {
                ps_t ps_new(ps);
                ps_new.accumulate(Ec);
                ps_new.push(segment(i,j-1, lWM, WM(i,j-1)));
                push_to_gstack(gstack, ps_new);
        }
}

void traceWMPrime(int i, int j, ps_t& ps, ps_stack_t& gstack) {
        for (int h = i+TURN+1 ; h <= j-TURN-2; h++) {
                if (WM(i,h-1) + WM(h,j) + ps.total() <= mfe + delta) {
                        ps_t ps_new(ps);
                        ps_new.push(segment(i,h-1, lWM, WM(i,h-1)));
                        ps_new.push(segment(h,j, lWM, WM(h,j)));
                        push_to_gstack(gstack, ps_new);
                }
        }
}

void traceVM(int i, int j, ps_t& ps, ps_stack_t& gstack) {
        int d3 = Ed3(i,j,j-1);
        int d5 = Ed5(i,j,i+1);

        if (WMPrime[i+1][j-1] + Ea + Eb + auPenalty(i, j) + d3 + d5 + ps.total() <= mfe + delta) {
                ps_t ps_new(ps);
                ps_new.accumulate(Ea + Eb + auPenalty(i, j) + d3 + d5); 
                ps_new.push(segment(i+1,j-1, lWMPrime,WMPrime[i+1][j-1] ));
                push_to_gstack(gstack, ps_new);
        }
}

void push_to_gstack(ps_stack_t& gstack, const ps_t& v) {
        gflag = 1;
        gstack.push(v);
}
