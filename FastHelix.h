#ifndef __FAST_HELIX__
#define __FAST_HELIX__

#include "GlobalPoint.h"
#include "FastCircle.h"
#include "FastLine.h"
#include <iostream>
/*
   Generation of track parameters at a vertex using two hits and a vertex.
   It is used e.g. by a seed generator.

   24.01.2012: introduced Maxpt cut. changed algo of "FastLine" to use vertex
   21.02.2001: Old FastHelix is now called FastHelixFit. Replace FastLineFit
               by FastLine (z0, dz/drphi calculated without vertex and errors)
   14.02.2001: Replace general Circle by FastCircle.
   13.02.2001: LinearFitErrorsInTwoCoordinates replaced by FastLineFit
   29.11.2000: (Pascal Vanlaer) Modification of calculation of sign of px,py
               and change in calculation of pz, z0.
   29.11.2000: (Matthias Winkler) Split stateAtVertex() in two parts (Circle
               is valid or not): helixStateAtVertex() and
                             straightLineStateAtVertex()
*/

class FastHelix
{
public:
    FastHelix(const GlobalPoint& oHit, const GlobalPoint& mHit, const GlobalPoint& aVertex,
              double nomField) :
        theCircle(oHit, mHit, aVertex)
    {
        tesla0 = 0.1 * nomField;
        maxRho = maxPt / (0.01 * 0.3 * tesla0);
        useBasisVertex = false;
        compute();
    }

    bool isValid() const
    {
        return theCircle.isValid();
    }

    const FastCircle & circle() const
    {
        return theCircle;
    }

    const GlobalPoint & getPt() const
    {
        return pt_;
    }

private:

    GlobalPoint const & outerHit() const
    {
        return theCircle.outerPoint();
    }
    GlobalPoint const & middleHit() const
    {
        return theCircle.innerPoint();
    }
    GlobalPoint const & vertex() const
    {
        return theCircle.vertexPoint();
    }

    void compute();
    void helixStateAtVertex();
    void straightLineStateAtVertex();

    static constexpr float maxPt = 10000; // 10Tev

    GlobalPoint pt_;

    GlobalPoint basisVertex;
    FastCircle theCircle;
    float tesla0;
    float maxRho;
    bool useBasisVertex;
    typedef int TrackCharge;
};

void FastHelix::compute()
{
    if (isValid() && (std::abs(tesla0) > 1e-3) && theCircle.rho() < maxRho) {
        helixStateAtVertex();
    } else {
        straightLineStateAtVertex();
    }
}

void FastHelix::helixStateAtVertex()
{
    // given the above rho>0.
    double rho = theCircle.rho();
    //remember (radius rho in cm):
    //rho =
    //100. * pt *
    //(10./(3.*MagneticField::inTesla(GlobalPoint(0., 0., 0.)).z()));
    // pt = 0.01 * rho * (0.3*MagneticField::inTesla(GlobalPoint(0.,0.,0.)).z());
    
    /*
    // this is the same as the circle fitter used on gpu
    const float BZ = 3.8112;
    // e = 1.602177×10^-19 C (coulombs)
    const float Q = 1.602177E-19;
    // c = 2.998×10^8 m/s (meters per second)
    //const float C = 2.998E8;
    // 1 GeV/c = 5.344286×10^-19 J s/m (joule seconds per meter)
    const float GEV_C = 5.344286E-19;
    std::cout << "PT CIRCLE " << Q * BZ * (rho * 1E-2) / GEV_C << std::endl;
    */

    double cm2GeV = 0.01 * 0.3 * tesla0;
    double pt = cm2GeV * rho;

    // verify that rho is not toooo large
    double dcphi = ((outerHit().x() - theCircle.x0()) * (middleHit().x() - theCircle.x0()) +
                    (outerHit().y() - theCircle.y0()) * (middleHit().y() - theCircle.y0())) / (rho * rho);

    if (std::abs(dcphi) >= 1.f) {
        straightLineStateAtVertex();
        return;
    }
    
    // tangent in v (or the opposite...)
    double px = -cm2GeV * (vertex().y() - theCircle.y0());
    double py =  cm2GeV * (vertex().x() - theCircle.x0());
    // check sign with scalar product
    if (px * (middleHit().x() - vertex().x()) + py * (middleHit().y() - vertex().y()) < 0.) {
        px = -px;
        py = -py;
    }

    //calculate z0, pz
    //(z, R*phi) linear relation in a helix
    //with R, phi defined as radius and angle w.r.t. centre of circle
    //in transverse plane
    //pz = pT*(dz/d(R*phi)))

    double dzdrphi = (outerHit().z() - middleHit().z()) / (rho * acos(dcphi));
    double pz = pt * dzdrphi;

    pt_[0] = px;
    pt_[1] = py;
    pt_[2] = pz;

    /*
    TrackCharge q = 1;
    if (theCircle.x0()*py - theCircle.y0()*px < 0) {
        q = -q;
    }
    if (tesla0 < 0.) {
        q = -q;
    }

    double z_0 =  middleHit().z();
    // assume v is before middleHit (opposite to outer)
    double ds = ((vertex().x() - theCircle.x0()) * (middleHit().x() - theCircle.x0()) +
                 (vertex().y() - theCircle.y0()) * (middleHit().y() - theCircle.y0())) / (rho * rho);
    if (std::abs(ds) < 1.f) {
        ds = rho * acos(ds);
        z_0 -= ds * dzdrphi;
    } else {
        // line????
        z_0 -= std::sqrt(perp2(middleHit() - vertex()) / perp2(outerHit() - middleHit())) *
               (outerHit().z() - middleHit().z());
    }
    */
}

void FastHelix::straightLineStateAtVertex()
{
    //calculate GlobalTrajectoryParameters assuming straight line...
    double dydx = 0.;
    double pt = 0., px = 0., py = 0.;

    if (fabs(theCircle.n1()) > 0. || fabs(theCircle.n2()) > 0.) {
        pt = maxPt  ;    // 10 TeV //else no pt
    }
    if (fabs(theCircle.n2()) > 0.) {
        dydx = -theCircle.n1() / theCircle.n2(); //else px = 0
    }
    px = pt / sqrt(1. + dydx * dydx);
    py = px * dydx;

    // check sign with scalar product
    if (px * (middleHit().x() - vertex().x()) + py * (middleHit().y() - vertex().y()) < 0.) {
        px *= -1.;
        py *= -1.;
    }

    //calculate z_0 and pz at vertex using weighted mean
    //z = z(r) = z0 + (dz/dr)*r
    //tan(theta) = dr/dz = (dz/dr)^-1
    //theta = atan(1./dzdr)
    //p = pt/sin(theta)
    //pz = p*cos(theta) = pt/tan(theta)

    FastLine flfit(outerHit(), middleHit());
    double dzdr = -flfit.n1() / flfit.n2();
    double pz = pt * dzdr;

    pt_[0] = px;
    pt_[1] = py;
    pt_[2] = pz;

    const float BZ = 3.8112;
    // e = 1.602177×10^-19 C (coulombs)
    const float Q = 1.602177E-19;
    // c = 2.998×10^8 m/s (meters per second)
    //const float C = 2.998E8;
    // 1 GeV/c = 5.344286×10^-19 J s/m (joule seconds per meter)
    const float GEV_C = 5.344286E-19;
    //std::cout << "PT CIRCLE " << Q * BZ * (rho * 1E-2) / GEV_C << std::endl;
    /*
    TrackCharge q = 1;

    double z_0 = -flfit.c() / flfit.n2();
    atVertex = GlobalTrajectoryParameters(GlobalPoint(vertex().x(), vertex().y(), z_0), GlobalPoint(px, py, pz), q);
    */
}

#endif
