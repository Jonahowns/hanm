#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_eigen.h>

#define MAX_ATOM 1000
#define SIZE (sizeof(double))
#define KBT 2.49434192

typedef struct{
    char type[6];
    int atomid;
    char name[4];
    char altloc;
    char resn[4];
    char chain;
    int resid;
    char icode;
    float x,y,z;
    float occupancy;
    float bfactor;
    char element[2];
    char charge[2];
}PDBLINE;

//tools for converting gsl_matrix, gsl_vector and double array
int array2matrix(double **a, gsl_matrix *matrix, int m, int n){
    size_t i,j;
    for(i=0;i<m;i++)
        for(j=0;j<n;j++)
            gsl_matrix_set(matrix,i,j,a[i][j]);
    return 0;
}
int matrix2array(gsl_matrix *matrix,double **a,int m,int n){
    size_t i,j;
    for(i=0;i<m;i++)
        for(j=0;j<n;j++)
            a[i][j]=gsl_matrix_get(matrix,i,j);
    return 0;
}
int vector2array(gsl_vector *vector, double *a, int n){
    size_t i;
    for(i=0;i<n;i++)
        a[i]=gsl_vector_get(vector,i);
    return 0;
}

int writeBfactor(double *bcal, int N, char *filename){
    //write B factors to Bcal.xvg
    FILE *fp;
    int i;
    if((fp=fopen(filename,"w"))==NULL){
        fprintf(stderr,"\n >> Error: Cannot write %s!\n",filename);
        exit(1);
    }
    for(i=0;i<N;i++){
        fprintf(fp,"%25.16f\n",bcal[i]);
    }
    fflush(fp);
    close(fp);
    return 0;
}

int writefc(double **fc, int N, int **flag, char *filename){
    FILE *fp;
    int k,l;
    if((fp=fopen(filename,"w"))==NULL){
        fprintf(stderr,"\n >> Error: Cannot write %s!\n",filename);
        exit(1);
    }
    for(k=0;k<N;k++){
        for(l=k+1;l<N;l++){
            if(flag[k][l]!=0)
                fprintf(fp,"%5d %5d %25.16f\n",k+1,l+1,fc[k][l]);
        }
    }
    fflush(fp);
    close(fp);
    return 0;
}

/*
int writecor(double **cor, int N, char *filename){
    FILE *fp;
    int i,j;
    if((fp=fopen(filename,"w"))==NULL){
        fprintf(stderr,"\n >> Error: Cannot write %s!\n",filename);
        exit(1);
    }
    for(i=0;i<3*N;i++){
        for(j=0;j<3*N;j++){
            fprintf(fp,"%12.6f  ",cor[i][j]);
        }
        fprintf(fp,"\n");
    }
    fflush(fp);
    close(fp);
    return 0;
}
*/

int writeeigvec(double **v, int N, char *filename){
    FILE *fp;
    int i,j;
    if((fp=fopen(filename,"w"))==NULL){
        fprintf(stderr,"\n >> Error: Cannot write %s!\n",filename);
        exit(1);
    }
    for(i=0;i<3*N;i++){
        for(j=0;j<3*N;j++){
            fprintf(fp,"%12.6f\n",v[i][j]);
        }
    }
    fflush(fp);
    close(fp);
    return 0;
}

int writeeigval(double *lambda, int N, char *filename){
    FILE *fp;
    int i;
    if((fp=fopen(filename,"w"))==NULL){
        fprintf(stderr,"\n >> Error: Cannot write %s!\n",filename);
        exit(1);
    }
    for(i=0;i<3*N;i++){
        fprintf(fp,"%12.6f\n",lambda[i]);
    }
    fflush(fp);
    close(fp);
    return 0;
}




int nma(int N, double *x, double *y, double *z, double **s,
        double *mass, double *sqrtmass, double **fc, double *fc_res,
        double **bond_fluc, double *bcal,
        double **H, double *lambda, double **v, double **cor,
        gsl_matrix *m, gsl_vector *eval, gsl_matrix *evec, gsl_eigen_symmv_workspace *w){

    int i,j,k;

    //calculate Hessian matrix
    for(i=0;i<3*N;i++){
        for(j=0;j<3*N;j++)
            H[i][j]=0;
    }
    for(i=0;i<N;i++){
        for(j=0;j<N;j++){
            if(j==i)
                continue;
            double xij,yij,zij,s2,fcij;
            xij=x[i]-x[j];
            yij=y[i]-y[j];
            zij=z[i]-z[j];
            s2=s[i][j]*s[i][j];
            fcij=fc[i][j];
            H[3*i][3*j]=-fcij*xij*xij/s2;
            H[3*i][3*j+1]=-fcij*xij*yij/s2;
            H[3*i][3*j+2]=-fcij*xij*zij/s2;
            H[3*i+1][3*j]=H[3*i][3*j+1];
            H[3*i+1][3*j+1]=-fcij*yij*yij/s2;
            H[3*i+1][3*j+2]=-fcij*yij*zij/s2;
            H[3*i+2][3*j]=H[3*i][3*j+2];
            H[3*i+2][3*j+1]=H[3*i+1][3*j+2];
            H[3*i+2][3*j+2]=-fcij*zij*zij/s2;

            H[3*i][3*i]+=-H[3*i][3*j];
            H[3*i][3*i+1]+=-H[3*i][3*j+1];
            H[3*i][3*i+2]+=-H[3*i][3*j+2];
            H[3*i+1][3*i]+=-H[3*i+1][3*j];
            H[3*i+1][3*i+1]+=-H[3*i+1][3*j+1];
            H[3*i+1][3*i+2]+=-H[3*i+1][3*j+2];
            H[3*i+2][3*i]+=-H[3*i+2][3*j];
            H[3*i+2][3*i+1]+=-H[3*i+2][3*j+1];
            H[3*i+2][3*i+2]+=-H[3*i+2][3*j+2];
        }
        H[3*i][3*i]+=fc_res[i];
        H[3*i+1][3*i+1]+=fc_res[i];
        H[3*i+2][3*i+2]+=fc_res[i];

    }
    //fprintf(stdout,"\n >> END of calculating Hessian Matrix!\n");

    //diagonalize Hessian matrix
    //mass weighted Hessian
    for(i=0;i<3*N;i++){
        for(j=0;j<3*N;j++){
            H[i][j]/=sqrtmass[i/3]*sqrtmass[j/3];
        }
    }
    array2matrix(H,m,3*N,3*N);
    gsl_eigen_symmv(m,eval,evec,w);
    gsl_eigen_symmv_sort(eval,evec,GSL_EIGEN_SORT_ABS_ASC);
    matrix2array(evec,v,3*N,3*N);   //2D array v saves eigen vectors in its columns
    vector2array(eval,lambda,3*N);

    //weight back eigen vectors
    for(i=0;i<3*N;i++){
        for(j=0;j<3*N;j++){
            v[i][j]/=sqrtmass[i/3];
        }
    }

    //calculate B factor and bond fluctuation
    double px,py,pz;
    for(i=0;i<N;i++){
        bcal[i]=0;
        for(j=6;j<3*N;j++){
            bcal[i]+=KBT*(v[3*i][j]*v[3*i][j]+v[3*i+1][j]*v[3*i+1][j]+v[3*i+2][j]*v[3*i+2][j])/lambda[j];
        }
        bcal[i]*=8*M_PI*M_PI/3*100;
    }
    for(i=0;i<N;i++){
        for(j=0;j<N;j++){
            bond_fluc[i][j]=0;
            for(k=6;k<3*N;k++){
                px=(x[i]-x[j])/s[i][j]*(v[3*i][k]-v[3*j][k]);
                py=(y[i]-y[j])/s[i][j]*(v[3*i+1][k]-v[3*j+1][k]);
                pz=(z[i]-z[j])/s[i][j]*(v[3*i+2][k]-v[3*j+2][k]);
                bond_fluc[i][j]+=(px+py+pz)*(px+py+pz)/lambda[k];
            }
        }
    }
    //correlation matrix
    /*
    for(i=0;i<3*N;i++){
        for(j=0;j<3*N;j++){
            cor[i][j]=0.0;
            for(k=6;k<3*N;k++){
                cor[i][j]+=v[i][k]*v[j][k]/sqrtmass[i/3]/sqrtmass[j/3]/lambda[k];
            }
            cor[i][j]*=KBT*100; //use A^2 as unit
        }
    }*/

    //end of calculating B factor and bond fluctuation
    return 0;
}

int main(int argc, char *argv[]){
    PDBLINE *pdb;
    char buff[200];
    FILE *fp;
    int i, j, k, l;

    //command line argvs
    char *pdbfile="ca.pdb";
    double k0=100;
    double cutoff=1.5;
    double factor=0.3;
    int mcycles,ncycles;
    char *fcfile="fc.xvg";

    int N;  //atom number
    double *x, *y, *z;  //coordinate vector
    double *mass,*sqrtmass;  //mass vector
    double **s, **fc;    //distance matrix and force constant matrix
    double **H; //Hessian Matrix
    double *lambda; //eigen values
    double **v; //eigen vectors
    double **cor;   //correlation matrix ~ pseodu inverse of Hessian
    int **flag;     //whether there is a bond for certain pair
    double *fc_res, *fc_res0;   //fc_res0 is a zero vector
    double **bond_fluc, **bond_fluc0, *bcal, *bexp, *bcal_prev;
    double ratioB1,ratioB2,beta,ddx2,ratiox2,alpha,BtoF;
    int bconv_mcycle1, bconv_mcycle2, bconv_ncycle;     //converge or not
    double threshold_mcycle1, threshold_mcycle2, threshold_ncycle;  //threhsold
    double tmp;

    //print system time
    system("date");

    //command line parameters
    if (argc!=7 && argc!=8){
        fprintf(stderr, "\n >> Usage: %s ca.pdb k0 cutoff factor mcycles ncycles (fcfile)!\n", argv[0]);
        fprintf(stderr,"\n Parameters are:");
        fprintf(stderr,"\n ca.pdb  --> PDB file containing coordinate of Ca atom");
        fprintf(stderr,"\n k0      --> Start with this uniform force constant, leave it 0 to use\n             the optimal force constant of ANM");
        fprintf(stderr,"\n cutoff  --> Atom pairs of distance within the cutoff will be connected\n             by a harmonic bond");
        fprintf(stderr,"\n factor  --> How large a restraint potential will be add in each cycle\n             we would use factor*3KBT*3/8pi^2*(Bcal-Bexp)/(Bcal*Bexp)");
        fprintf(stderr,"\n mcycles --> Number of outer cycles, updating B factors");
        fprintf(stderr,"\n ncycles --> Number of Inner cycles, fluctuation matching");
        fprintf(stderr,"\n fcfiles --> (Optional) Provide a force constant file to use values inside\n");
        exit(1);
    }
    //print out command line (save in log file)
    fprintf(stdout, "cmdline: ");
    for(i=0;i<argc;i++)
        fprintf(stdout,"%s ",argv[i]);
    fprintf(stdout, "\n");

    pdbfile=argv[1];
    k0=atof(argv[2]);
    cutoff=atof(argv[3]);
    factor=atof(argv[4]);
    mcycles=atoi(argv[5]);
    ncycles=atoi(argv[6]);
    if (argc==8)
        fcfile=argv[7];

    //iteration parameters
    threshold_mcycle1=0.005;     //relative
    threshold_mcycle2=0.01;     //absolute
    threshold_ncycle=0.001;
    BtoF=3/(800*M_PI*M_PI);
    alpha=0.5;


    //read in pdb coordinates & B factors
    pdb=(PDBLINE *)malloc(sizeof(PDBLINE)*MAX_ATOM);
    if((fp=fopen(pdbfile,"r"))==NULL){
        fprintf(stderr,"\n >> Error: Cannot open PDB file: %s\n", pdbfile);
        exit(1);
    }
    N=0;
    char *type=(char *)malloc(sizeof(char)*200);
    while(fgets(buff,200,fp)!=NULL){
        type=strncpy(type,buff,4);
        type[4]='\0';
        if(strcmp(type,"ATOM")!=0)
            continue;
        else{
            sscanf(buff,"%6s%5d %4s%c%3s %c%4d%c   %f%f%f%f%f          %2s%2s",pdb[N].type,&pdb[N].atomid,pdb[N].name,&pdb[N].altloc,pdb[N].resn,&pdb[N].chain,&pdb[N].resid,&pdb[N].icode,&pdb[N].x,&pdb[N].y,&pdb[N].z,&pdb[N].occupancy,&pdb[N].bfactor,pdb[N].element,pdb[N].charge);
            /* This part is added because some B factors larger than 100 will
             * become connected with the previous occupancy data, thus lead to
             * incorrect read in of B factor */
            type=strncpy(type,buff+60,6);
            type[6]='\0';
            pdb[N].bfactor=atof(type);
            //fprintf(stderr,"%-6s%5d %4s%c%3s %c%4d%c   %8.3f%8.3f%8.3f%6.2f%6.2f          %2s%2s\n",pdb[N].type,pdb[N].atomid,pdb[N].name,pdb[N].altloc,pdb[N].resn,pdb[N].chain,pdb[N].resid,pdb[N].icode,pdb[N].x,pdb[N].y,pdb[N].z,pdb[N].occupancy,pdb[N].bfactor,pdb[N].element,pdb[N].charge);
            N++;
        }
    }
    free(type);
    fclose(fp);
    x=(double *)malloc(SIZE*N);
    y=(double *)malloc(SIZE*N);
    z=(double *)malloc(SIZE*N);
    bexp=(double *)malloc(SIZE*N);
    mass=(double *)malloc(SIZE*N);
    sqrtmass=(double *)malloc(SIZE*N);
    for(i=0;i<N;i++){
        x[i]=pdb[i].x/10.0;
        y[i]=pdb[i].y/10.0;
        z[i]=pdb[i].z/10.0;
        bexp[i]=pdb[i].bfactor;
        switch (pdb[i].name[0]){
            case 'P': {mass[i]=30.974;break;}
            case 'C': {mass[i]=12.011;break;}
            case 'N': {mass[i]=14.007;break;}
            case 'O': {mass[i]=15.999;break;}
            case 'S': {mass[i]=32.060;break;}
            case 'H': {mass[i]=1.008;break;}
            default: {fprintf(stderr,"\n >> PDB file contains unsupported atoms!");}
        }
    }
    for(i=0;i<N;i++)
        sqrtmass[i]=sqrt(mass[i]);
    fprintf(stdout,"\n >> END of read in pdb!\n");
    //return 0;

    //calculate distance matrix from coordinates
    s=(double **)malloc(sizeof(double *)*N);
    for(i=0;i<N;i++){
        s[i]=(double *)malloc(SIZE*N);
        for(j=0;j<N;j++){
            s[i][j]=sqrt((x[i]-x[j])*(x[i]-x[j])+
                         (y[i]-y[j])*(y[i]-y[j])+
                         (z[i]-z[j])*(z[i]-z[j]));
        }
    }

    //assign force constant
    fc=(double **)malloc(sizeof(double *)*N);
    flag=(int **)malloc(sizeof(int *)*N);
    fc_res=(double *)malloc(SIZE*N);
    fc_res0=(double *)malloc(SIZE*N);
    for(i=0;i<N;i++){
        fc[i]=(double *)malloc(SIZE*N);
        flag[i]=(int *)malloc(sizeof(int)*N);
        fc_res[i]=0;
        fc_res0[i]=0;
        for(j=0;j<N;j++){
            fc[i][j]=0;
            flag[i][j]=0;
            if(argc==8)
                continue;
            if(s[i][j]<cutoff && j!=i){
                /* if k0=0, means initial fc not known*/
                if (k0<1e-5)
                    fc[i][j]=1;
                else
                    fc[i][j]=k0;
                flag[i][j]=1;
            }
        }
    }

    //allocate memory for matrixs and vectors
    H=(double **)malloc(sizeof(double *)*3*N);
    lambda=(double *)malloc(SIZE*3*N);
    v=(double **)malloc(sizeof(double *)*3*N);
    cor=(double **)malloc(sizeof(double *)*3*N);
    for(i=0;i<3*N;i++){
        H[i]=(double *)malloc(SIZE*3*N);
        cor[i]=(double *)malloc(SIZE*3*N);
        v[i]=(double *)malloc(SIZE*3*N);
    }
    bcal=(double *)calloc(N,SIZE);
    bcal_prev=(double *)calloc(N,SIZE);
    bond_fluc=(double **)malloc(sizeof(double *)*N);
    bond_fluc0=(double **)malloc(sizeof(double *)*N);
    for(i=0;i<N;i++){
        bond_fluc[i]=(double *)calloc(N,SIZE);
        bond_fluc0[i]=(double *)calloc(N,SIZE);
    }
    gsl_matrix *m=gsl_matrix_alloc(3*N,3*N);
    gsl_vector *eval=gsl_vector_alloc(3*N);
    gsl_matrix *evec=gsl_matrix_alloc(3*N,3*N);
    gsl_eigen_symmv_workspace *w=gsl_eigen_symmv_alloc(3*N);

    /* if fc is given by a file, read in & assign */
    if(argc==8){
        if((fp=fopen(fcfile,"r"))==NULL){
            fprintf(stderr,"\n >> Error: Cannot open force constant file: %s\n", fcfile);
            exit(1);
        }
        while(fgets(buff,200,fp)!=NULL){
            sscanf(buff,"%d%d%lf",&i,&j,&tmp);
            i--;j--;
            fc[i][j]=tmp;
            fc[j][i]=tmp;
            flag[i][j]=1;
            flag[i][j]=1;
        }
        fclose(fp);
    }
        /* if initial fc is given as 0, use following part to calculate it */
    else if(k0<1e-5){
        nma(N,x,y,z,s,mass,sqrtmass,fc,fc_res0,bond_fluc,bcal,H,lambda,v,cor,m,eval,evec,w);
        double tmp1=0.0;
        double tmp2=0.0;
        for(i=0;i<N;i++){
            tmp1+=bcal[i]*bexp[i];
            tmp2+=bcal[i]*bcal[i];
        }
        k0=tmp2/tmp1;
        for(i=0;i<N;i++){
            for(j=0;j<N;j++){
                if(flag[i][j]==1)
                    fc[i][j]=k0;
            }
        }
    }

    //loops to optimize force constant
    //calculate all quantities using current force constant
    nma(N,x,y,z,s,mass,sqrtmass,fc,fc_res0,bond_fluc,bcal,H,lambda,v,cor,m,eval,evec,w);
    sprintf(buff,"Bcal_0.xvg");
    writeBfactor(bcal,N,buff);
    sprintf(buff,"fc_0.xvg");
    writefc(fc,N,flag,buff);
    //sprintf(buff,"cor_0.mtx");
    //writecor(cor,N,buff);
    //sprintf(buff,"eigval_0.xvg");
    //writeeigval(lambda,N,buff);
    //sprintf(buff,"eigvec_0.xvg");
    //writeeigvec(v,N,buff);
    fprintf(stdout,"k0=%12.6f\n",k0);

    //main loop
    for(i=1;i<=mcycles;i++){
        //check convergence by comparing Bcal with Bexp & Bcal_prev
        //if not converged, cal fc_res by difference of bcal and bexp
        bconv_mcycle1=1;
        bconv_mcycle2=1;
        for(j=0;j<N;j++){
            ratioB1=fabs(bcal[j]-bcal_prev[j])/bcal[j]; //relative
            ratioB2=fabs(bcal[j]-bexp[j])/bexp[j];      //absolute
            if(ratioB1>threshold_mcycle1) bconv_mcycle1=0;
            if(ratioB2>threshold_mcycle2) bconv_mcycle2=0;
            //save bcal to bcal_prev
            bcal_prev[j]=bcal[j];

            fc_res[j]=factor*3*KBT*(bcal[j]-bexp[j])/(bcal[j]*bexp[j]*BtoF);
            //fprintf(stdout,"beta[%d]=%12.6f,fc_res[%d]=%12.6f\n",j,beta,j,fc_res[j]);
        }
        //convergence criterion set here, modify this part if you want to use
        //a different criterion. For example:
        //if (bconv_mcycle1 || bconv_mcycle2){
        if (bconv_mcycle1){
            fprintf(stdout,"\n >> The Bfactors have converged in %d mcycles!\n",i);
            break;
        }
        else
            fprintf(stdout,"\n >> The Bfactors have not converged in %d mcycles!\n",i);
        //call nma routine and save bond fluctuation into bond_fluc0
        //this is used as standard fluctuation for inner loop of fluctuation matching
        nma(N,x,y,z,s,mass,sqrtmass,fc,fc_res,bond_fluc0,bcal,H,lambda,v,cor,m,eval,evec,w);

        //update fc by fluctuation matching
        for(j=1;j<=ncycles;j++){
            //calculate bond_fluc
            nma(N,x,y,z,s,mass,sqrtmass,fc,fc_res0,bond_fluc,bcal,H,lambda,v,cor,m,eval,evec,w);
            //check convergency and update fc
            bconv_ncycle=1;
            //fprintf(stdout,"\n >> ncycle=%d\n>> \tk\tl\tfluc[kl]\tddx2\tratiox2\tfc[kl]",j);
            for(k=0;k<N;k++){
                for(l=k+1;l<N;l++){
                    if(flag[k][l]!=0){
                        ddx2=bond_fluc[k][l]-bond_fluc0[k][l];
                        ratiox2=fabs(ddx2)/bond_fluc0[k][l];
                        if(ratiox2>threshold_ncycle) bconv_ncycle=0;

                        fc[k][l]=1.0/((1.0/fc[k][l])-alpha*ddx2);
                        fc[l][k]=fc[k][l];
                        //fprintf(stdout,"\n%5d %5d %24.16f %24.16f %24.16f %12.6f",k,l,bond_fluc[k][l],ddx2,ratiox2,fc[k][l]);
                    }
                }
            }
            if(bconv_ncycle){
                fprintf(stdout,"\n >> The FC have converged after %d ncycles in the %d mcycles!\n",j,i);
                break;
            }
            else
                fprintf(stdout,"\n >> The FC have not converged after %d ncycles in the %d mcycles!\n",j,i);
        }

        //save result every 10 mcycles
        //Modify here if you want more frequent output
        if(i%10==0){
            sprintf(buff,"Bcal_%d.xvg",i);
            writeBfactor(bcal,N,buff);
            sprintf(buff,"fc_%d.xvg",i);
            writefc(fc,N,flag,buff);
            //sprintf(buff,"cor_%d.mtx",i);
            //writecor(cor,N,buff);
        }

    }
    //save final result
    sprintf(buff,"Bcal_final.xvg");
    writeBfactor(bcal,N,buff);
    sprintf(buff,"fc_final.xvg");
    writefc(fc,N,flag,buff);
    //sprintf(buff,"cor_final.mtx");
    //writecor(cor,N,buff);
    //sprintf(buff,"eigval_final.xvg");
    //writeeigval(lambda,N,buff);
    //sprintf(buff,"eigvec_final.xvg");
    //writeeigvec(v,N,buff);

    fprintf(stdout,"\n >> Normal Termination! << \n");

    //free pointers
    free(pdb);
    free(x);
    free(y);
    free(z);
    free(s);
    free(fc);
    free(bond_fluc);
    free(bond_fluc0);
    free(bexp);
    free(bcal);
    gsl_eigen_symmv_free(w);
    gsl_matrix_free(m);
    gsl_matrix_free(evec);
    gsl_vector_free(eval);

    system("date");
    return 0;
}


