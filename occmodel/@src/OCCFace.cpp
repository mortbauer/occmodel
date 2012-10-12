// Copyright 2012 by Runar Tenfjord, Tenko as.
// See LICENSE.txt for details on conditions.
#include "OCCModel.h"

OCCFace *OCCFace::copy(bool deepCopy = false)
{
    OCCFace *ret = new OCCFace();
    try {
        if (deepCopy) {
            BRepBuilderAPI_Copy A;
            A.Perform(this->getShape());
            ret->setShape(A.Shape());
        } else {
            ret->setShape(this->getShape());
        }
    } catch(Standard_Failure &err) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        const Standard_CString msg = e->GetMessageString();
        if (msg != NULL && strlen(msg) > 1) {
            setErrorMessage(msg);
        } else {
            setErrorMessage("Failed to copy face");
        }
        return NULL;
    }
    return ret;
}

int OCCFace::numFaces()
{
    TopTools_IndexedMapOfShape anIndices;
    const TopoDS_Shape& shp = this->getShape();
    if (shp.ShapeType() == TopAbs_FACE) {
        return 1;
    } else {
        // Shell
        TopExp::MapShapes(shp, TopAbs_FACE, anIndices);
        return anIndices.Extent();
    }
}

int OCCFace::numWires()
{
    TopTools_IndexedMapOfShape anIndices;
    TopExp::MapShapes(this->getShape(), TopAbs_WIRE, anIndices);
    return anIndices.Extent();
}

int OCCFace::createFace(std::vector<OCCWire *> wires) {
    try {
        const TopoDS_Wire& outerwire = wires[0]->getWire();
        BRepBuilderAPI_MakeFace MF(outerwire);
        
        // add optional holes
        for (unsigned i = 1; i < wires.size(); i++) {
            const TopoDS_Wire& wire = wires[i]->getWire();
            if (wire.Orientation() != outerwire.Orientation()) {
                MF.Add(TopoDS::Wire(wire.Reversed()));
            } else {
                MF.Add(wire);
            }
        }
        this->setShape(MF.Shape());
        
        // possible fix shape
        if (!this->fixShape())
            StdFail_NotDone::Raise("Shapes not valid");
        
    } catch(Standard_Failure &err) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        const Standard_CString msg = e->GetMessageString();
        if (msg != NULL && strlen(msg) > 1) {
            setErrorMessage(msg);
        } else {
            setErrorMessage("Failed to create face");
        }
        return 1;
    }
    return 0;
}

int OCCFace::createConstrained(std::vector<OCCEdge *> edges, std::vector<DVec> points) {
    try {
        BRepOffsetAPI_MakeFilling aGenerator;
        for (unsigned i = 0; i < edges.size(); i++) {
            OCCEdge *edge = edges[i];
            aGenerator.Add(edge->edge, GeomAbs_C0);
        }
        for (unsigned i = 0; i < points.size(); i++) {
            gp_Pnt aPnt(points[i][0], points[i][1], points[i][2]);
            aGenerator.Add(aPnt);
        }
        aGenerator.Build();
        this->setShape(aGenerator.Shape());
        
        // possible fix shape
        if (!this->fixShape())
            StdFail_NotDone::Raise("Shapes not valid");
        
    } catch(Standard_Failure &err) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        const Standard_CString msg = e->GetMessageString();
        if (msg != NULL && strlen(msg) > 1) {
            setErrorMessage(msg);
        } else {
            setErrorMessage("Failed to create face");
        }
        return 1;
    }
    return 0;
}

double OCCFace::area() {
    GProp_GProps prop;
    BRepGProp::SurfaceProperties(this->getShape(), prop);
    return prop.Mass();
}

DVec OCCFace::inertia() {
    DVec ret;
    GProp_GProps prop;
    BRepGProp::SurfaceProperties(this->getShape(), prop);
    gp_Mat mat = prop.MatrixOfInertia();
    ret.push_back(mat(1,1)); // Ixx
    ret.push_back(mat(2,2)); // Iyy
    ret.push_back(mat(3,3)); // Izz
    ret.push_back(mat(1,2)); // Ixy
    ret.push_back(mat(1,3)); // Ixz
    ret.push_back(mat(2,3)); // Iyz
    return ret;
}

DVec OCCFace::centreOfMass() {
    DVec ret;
    GProp_GProps prop;
    BRepGProp::SurfaceProperties(this->getShape(), prop);
    gp_Pnt cg = prop.CentreOfMass();
    ret.push_back(cg.X());
    ret.push_back(cg.Y());
    ret.push_back(cg.Z());
    return ret;
}

int OCCFace::offset(double offset, double tolerance = 1e-6) {
    try {
        BRepOffset_MakeOffset MO(this->getShape(), offset, tolerance, BRepOffset_Skin,
                                 Standard_False, Standard_False, GeomAbs_Intersection, Standard_False);
        
        if (!MO.IsDone()) {
            StdFail_NotDone::Raise("Failed to offset face");
        }
        
        const TopoDS_Shape& tmp = MO.Shape();
        BRepCheck_Analyzer aChecker(tmp);
        
        if (tmp.IsNull() || !aChecker.IsValid()) {
            StdFail_NotDone::Raise("offset result not valid");
        }
        
        this->setShape(tmp);
        
        // possible fix shape
        if (!this->fixShape())
            StdFail_NotDone::Raise("Shapes not valid");
        
    } catch(Standard_Failure &err) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        const Standard_CString msg = e->GetMessageString();
        if (msg != NULL && strlen(msg) > 1) {
            setErrorMessage(msg);
        } else {
            setErrorMessage("Failed to offset face");
        }
        return 1;
    }
    return 0;
}

int OCCFace::createPolygonal(std::vector<DVec> points)
{
    try {
        BRepBuilderAPI_MakePolygon MP;
        for (unsigned i=0; i<points.size(); i++) {
            MP.Add(gp_Pnt(points[i][0], points[i][1], points[i][2]));
        }
        MP.Close();
        if (!MP.IsDone()) {
            StdFail_NotDone::Raise("failed to create face");;
        }
        BRepBuilderAPI_MakeFace MF(MP.Wire(), false);
        this->setShape(MF.Face());
        
        // possible fix shape
        if (!this->fixShape())
            StdFail_NotDone::Raise("Shapes not valid");
        
    } catch(Standard_Failure &err) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        const Standard_CString msg = e->GetMessageString();
        if (msg != NULL && strlen(msg) > 1) {
            setErrorMessage(msg);
        } else {
            setErrorMessage("Failed to create face");
        }
        return 1;
    }
    return 0;
}

int OCCFace::extrude(OCCBase *shape, DVec p1, DVec p2) {
    try {
        const TopoDS_Shape& shp = shape->getShape();
        // Only accept Edge or Wire
        TopAbs_ShapeEnum type = shp.ShapeType();
        if (type != TopAbs_EDGE && type != TopAbs_WIRE) {
            StdFail_NotDone::Raise("expected Edge or Wire");
        }
        
        gp_Vec direction(gp_Pnt(p1[0], p1[1], p1[2]),
                         gp_Pnt(p2[0], p2[1], p2[2]));
        gp_Ax1 axisOfRevolution(gp_Pnt(p1[0], p1[1], p1[2]), direction);
        
        BRepPrimAPI_MakePrism MP(shp, direction, Standard_False);
        this->setShape(MP.Shape());
        
        // possible fix shape
        if (!this->fixShape())
            StdFail_NotDone::Raise("Shapes not valid");
        
    } catch(Standard_Failure &err) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        const Standard_CString msg = e->GetMessageString();
        if (msg != NULL && strlen(msg) > 1) {
            setErrorMessage(msg);
        } else {
            setErrorMessage("Failed to extrude");
        }
        return 1;
    }
    return 0;
}

int OCCFace::revolve(OCCBase *shape, DVec p1, DVec p2, double angle)
{
    try {
        const TopoDS_Shape& shp = shape->getShape();
        // Only accept Edge or Wire
        TopAbs_ShapeEnum type = shp.ShapeType();
        if (type != TopAbs_EDGE && type != TopAbs_WIRE) {
            StdFail_NotDone::Raise("Expected Edge or Wire");
        }
        
        gp_Dir direction(p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2]);
        gp_Ax1 axisOfRevolution(gp_Pnt(p1[0], p1[1], p1[2]), direction);
        
        BRepPrimAPI_MakeRevol MR(shp, axisOfRevolution, angle, Standard_False);
        if (!MR.IsDone()) {
            StdFail_NotDone::Raise("Failed in revolve operation");;
        }
        this->setShape(MR.Shape());
        
        // possible fix shape
        if (!this->fixShape())
            StdFail_NotDone::Raise("Shapes not valid");
        
    } catch(Standard_Failure &err) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        const Standard_CString msg = e->GetMessageString();
        if (msg != NULL && strlen(msg) > 1) {
            setErrorMessage(msg);
        } else {
            setErrorMessage("Failed to revolve");
        }
        return 1;
    }
    return 0;
}

int OCCFace::sweep(OCCWire *spine, std::vector<OCCBase *> profiles, int cornerMode = 0)
{
    try {
        BRepOffsetAPI_MakePipeShell PS(spine->getWire());
        // set corner mode
        switch (cornerMode) {
            case 1: PS.SetTransitionMode(BRepBuilderAPI_RightCorner);
                break;
            case 2: PS.SetTransitionMode(BRepBuilderAPI_RoundCorner);
                break;
            default: PS.SetTransitionMode(BRepBuilderAPI_Transformed);
                break;
        }
        // add profiles
        for (unsigned i=0; i<profiles.size(); i++) {
            PS.Add(profiles[i]->getShape());
        }
        if (!PS.IsReady()) {
            StdFail_NotDone::Raise("Failed in sweep operation");
        }
        PS.Build();
        
        this->setShape(PS.Shape());
        
        // possible fix shape
        if (!this->fixShape())
            StdFail_NotDone::Raise("Shapes not valid");
        
    } catch(Standard_Failure &err) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        const Standard_CString msg = e->GetMessageString();
        if (msg != NULL && strlen(msg) > 1) {
            setErrorMessage(msg);
        } else {
            setErrorMessage("Failed to sweep");
        }
        return 1;
    }
    return 0;
}

int OCCFace::loft(std::vector<OCCBase *> profiles, bool ruled, double tolerance)
{
    try {
        Standard_Boolean isSolid = Standard_False;
        Standard_Boolean isRuled = Standard_True;
        
        if (!ruled) isRuled = Standard_False;
        
        BRepOffsetAPI_ThruSections TS(isSolid, isRuled, tolerance);
        
        for (unsigned i=0; i<profiles.size(); i++) {
            if (profiles[i]->getShape().ShapeType() == TopAbs_WIRE) {
                TS.AddWire(TopoDS::Wire(profiles[i]->getShape()));
            } else {
                TS.AddVertex(TopoDS::Vertex(profiles[i]->getShape()));
            }
        }
        //TS.CheckCompatibility(Standard_False);  
        TS.Build();
        if (!TS.IsDone()) {
            StdFail_NotDone::Raise("Failed in loft operation");;
        }
        this->setShape(TS.Shape());
        
        // possible fix shape
        if (!this->fixShape())
            StdFail_NotDone::Raise("Shapes not valid");
        
    } catch(Standard_Failure &err) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        const Standard_CString msg = e->GetMessageString();
        if (msg != NULL && strlen(msg) > 1) {
            setErrorMessage(msg);
        } else {
            setErrorMessage("Failed to loft");
        }
        return 1;
    }
    return 0;
}

int OCCFace::boolean(OCCSolid *tool, BoolOpType op) {
    try {
        TopoDS_Shape shape;
        switch (op) {
            case BOOL_CUT:
            {
                BRepAlgoAPI_Cut CU (this->getShape(), tool->getShape());
                if (!CU.IsDone())
                    Standard_ConstructionError::Raise("operation failed");
                shape = CU.Shape();
                break;
            }
            case BOOL_COMMON:
            {
                BRepAlgoAPI_Common CO (this->getShape(), tool->getShape());
                if (!CO.IsDone())
                    Standard_ConstructionError::Raise("operation failed");
                shape = CO.Shape();
                break;
            }
            default:
                Standard_ConstructionError::Raise("unknown operation");
                break;
        }
        // extract single face or single shell
        int idx = 0;
        TopExp_Explorer exBO;
        for (exBO.Init(shape, TopAbs_SHELL); exBO.More(); exBO.Next()) {
            if (idx > 0) {
                Standard_ConstructionError::Raise("multiple object in result");
            }
            const TopoDS_Shape& cur = exBO.Current();
            this->setShape(cur);
            idx++;
        }
        if (idx == 0) {
            idx = 0;
            for (exBO.Init(shape, TopAbs_FACE); exBO.More(); exBO.Next()) {
                if (idx > 0) {
                    Standard_ConstructionError::Raise("multiple object in result");
                }
                const TopoDS_Shape& cur = exBO.Current();
                this->setShape(cur);
                idx++;
            }
        }
        if (idx == 0)
            StdFail_NotDone::Raise("no results from boolean operation");;
        this->setShape(shape);
        
        // possible fix shape
        if (!this->fixShape())
            StdFail_NotDone::Raise("Shapes not valid");
        
    } catch(Standard_Failure &err) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        const Standard_CString msg = e->GetMessageString();
        if (msg != NULL && strlen(msg) > 1) {
            setErrorMessage(msg);
        } else {
            setErrorMessage("Failed in boolean operation");
        }
        return 1;
    }
    return 0;
}

OCCMesh *OCCFace::createMesh(double factor, double angle, bool qualityNormals = true)
{
    OCCMesh *mesh = new OCCMesh();
    
    try {
        Bnd_Box aBox;
        BRepBndLib::Add(this->getShape(), aBox);
        
        Standard_Real aXmin, aYmin, aZmin;
        Standard_Real aXmax, aYmax, aZmax;
        aBox.Get(aXmin, aYmin, aZmin, aXmax, aYmax, aZmax);
        
        Standard_Real maxd = fabs(aXmax - aXmin);
        maxd = std::max(maxd, fabs(aYmax - aYmin));
        maxd = std::max(maxd, fabs(aZmax - aZmin));
        
        BRepMesh_FastDiscret MSH(factor*maxd, angle, aBox, Standard_False, Standard_False, 
                                 Standard_True, Standard_True);
        
        MSH.Perform(this->getShape());
        
        BRepMesh::Mesh(this->getShape(),factor*maxd);
        
        if (this->getShape().ShapeType() != TopAbs_FACE) {
            TopExp_Explorer exFace;
            for (exFace.Init(this->getShape(), TopAbs_FACE); exFace.More(); exFace.Next()) {
                const TopoDS_Face& faceref = static_cast<const TopoDS_Face &>(exFace.Current());
                mesh->extractFaceMesh(faceref, qualityNormals);
            }
        } else {
            mesh->extractFaceMesh(this->getFace(), qualityNormals);
        }
    } catch(Standard_Failure &err) {
        Handle_Standard_Failure e = Standard_Failure::Caught();
        const Standard_CString msg = e->GetMessageString();
        if (msg != NULL && strlen(msg) > 1) {
            setErrorMessage(msg);
        } else {
            setErrorMessage("Failed to create mesh");
        }
        return NULL;
    }
    return mesh;
}