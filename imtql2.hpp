#ifndef __HEADER_IMTQL2_HPP__
#define __HEADER_IMTQL2_HPP__


template <class T>
__device__ __noinline__ int
//__device__  __forceinline__ int
imtql2_( const int nm, const int n,
                T * __restrict__ d_, T * __restrict__ e_, T * __restrict__ z_,
                // optional arguments
                int const MAX_SWEEP = 10,
                T const tol = std::numeric_limits<T>::epsilon()*(std::is_same<T,double>::value?512:16),
                bool const DO_SORT = false
        )        
{
  const int myid = threadIdx.x % WARP_GPU_SIZE + 1;
#define        z(row,col)        (*(z_+((row)-1)+((col)-1)*nm))
#define        d(index)        (*(d_+((index)-1)))
#define        e(index)        (*(e_+((index)-1)))
#define        pos(index)        (*(pos_+((index)-1)))

  const int tile_size = WARP_GPU_SIZE;
  T * shmem = __SHMEM__();

  const T ZERO = static_cast<T>(0.0e0);
  const T ONE  = static_cast<T>(1.0e0);


  int ierror = 0;
  _if_ (n == 1) { return ierror; }

  #pragma unroll 1
  for(int i=1; i<=n; i++) {
    #pragma unroll 1
    for(int j=myid; j<=n; j+=WARP_GPU_SIZE) {
      z(j, i) = ZERO;
    }
  }
  #pragma unroll 1
  for(int i=myid; i<=n; i+=WARP_GPU_SIZE) {
    z(i, i) = ONE;
  } sync_over_warp();

  #pragma unroll 1
  for(int l=1; l<=n; l++) { // most outer loop

    int itr;
    #pragma unroll 1
    for (itr=0; itr<=MAX_SWEEP; itr++) {

      int m;
      {
        T g = Abs(d(l));
        #pragma unroll 1
        for(m=l; m<=n-1; m++) {
          const T f = Abs(d(m+1));
          const T tst1 = (g + f)*tol;
          const T tst2 = Abs(e(m));
          _if_ (tst2 <= tst1) break;
          g = f;
        }
      }
      _if_ (m == l) break; // convergence

      T di1, r;
      {
        const T dl = d(l);
        const T dl1 = d(l+1);
        const T el = e(l);
        const T f = Div(dl1 - dl, el+el);
        const T g = pythag1(f);
        di1 = d(m);
        r = (di1 - dl) + Div(el, f + Sign(g, f));
      }

      T s = ONE;
      T c = ONE;
      T delta_d = ZERO;

      int i;
      #pragma unroll 1
      for(i=m-1; i>=l; i--) {

        const T ei = e(i);
        const T f = s * ei;
        const T g = r;
        r = pythag(f, r);
        _if_ (r == ZERO) break;

        const T b = c * ei;
        s = Div(f, r);
        c = Div(g, r);

        const T dix = di1 - delta_d;
        di1 = d(i);
        const T q = (di1 - dix) * s + 2 * c * b;
        const T dx1 = fma(s, q, dix);
        delta_d = dx1 - dix;
        sync_over_warp();
        _if_ (myid==1) { e(i+1) = r; d(i+1) = dx1; }
        r = c * q - b;

        {
          T * zki0_ptr = &z(myid,i+0);
          T * zki1_ptr = &z(myid,i+1);
          #pragma unroll 1
          for(int k=myid; k<=n; k+=WARP_GPU_SIZE) {
            const T f0 = *zki0_ptr;
            const T f1 = *zki1_ptr;
            *zki1_ptr = s * f0 + c * f1;
            *zki0_ptr = c * f0 - s * f1;
            zki0_ptr+=WARP_GPU_SIZE; zki1_ptr+=WARP_GPU_SIZE;
          }
        }

      } sync_over_warp();

      _if_ (myid == 1) {
        const int j = max(i+1,l);
        d(j) -= delta_d;
        e(j) = r;
        e(m) = ZERO;
      }
    }
    _if_ (itr>MAX_SWEEP) { ierror = l; break; }

  } sync_over_warp();

  _if_ ( DO_SORT ) {
    _if_ ( ierror == 0 ) {
      int * const pos_ = (int *)e_;
      for(int i=myid; i<=n; i+=WARP_GPU_SIZE) {
        pos(i) = i;
      } sync_over_warp();
      #pragma unroll 1
      for(int i=2; i<=n; i++) {
        const int l = i - 1;
        _if_ (myid==1) {
          T dl = d(l);
          int il = l;
          #pragma unroll 1
          for (int j=i; j<=n; j++) {
            const T dj = d(j);
            const bool flag = dl > dj;
            __UPDATE__(dl, dj, flag);
            __UPDATE__(il, j, flag);
          }
          _if_ (il!=l) {
            int p=pos(l); pos(l)=pos(il); pos(il)=p;
            d(il)=d(l); d(l)=dl;
          }
        }
      } sync_over_warp();
    }
  }

#undef        z
#undef        d
#undef        e
  sync_over_warp();
  return ierror;
}

#endif
