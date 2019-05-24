---
layout: default
---

# Input Files

## Introduction
OpenMKM uses two input files supplied as two arguments to the OpenMKM executable.

1. The first argument *rctr.yaml* is the name of yaml file specifying the reactor model parameters, 
operating conditions, and the names of thermodynamic phases (which are defined in the Cantera XML file 
supplied as second argument) and the starting composition and coverages of gas and surface phases 
respectively. For more details on the yaml file format, refer to the YAML subsection below.

2. The second argument *input.xml* is Cantera input file in XML format which provides the 
definitions of species, reactions, interactions, and gas, solid and  catalyst surface phases. 
Cantera defines an additional input format called CTI, which is easier to work with.
To assist those migrating from Chemkin, scripts are available to convert Chemkin input files to Cantera files. 

## YAML Input File.

The first argument (called *rctr.yaml* throughout this document, but could be named anything) 
uses [YAML](https://yaml.org) format, which is a human friendly data format. YAML is a superset of 
JSON format, so the input could be specified in JSON format also. The data specified in YAML file can 
be thought of as a nested python  dictionary. A brief snippet of a pseudo YAML file containing 
most of the YAML format features used for *rctr.yaml* are given below.

```python
key1 : value1
key2 : 
    nested_key1: value2
    nested_key2:
       - array_value1
       - array_value2
```

In the above snippet, the YAML data contains two level-1 keys (also called YAML nodes), *key1, key2*.
*key1* has a single data value, *value1* assigned to it. On the other hand *key2* consists of 
nested compound data (which is again a dictionary) as value. The keys of nested dictionary can be
thought of as level-2 keys. Intersting would be the value for 
*nested\_key2*, which is a sequence (also called array) of two values, 
*array\_value1* and *array\_value2*. 

YAML is a free format. So *key1 : value1* line could have been given below the data for *key2*. Also any text after *\#* is a comment.

*rctr.yaml* specifies 3 mandatory and one optional level-1 nodes (or keys). The 
mandatory nodes are *reactor*, *simulation*, *phases*, and the optional node is 
*inlet_gas*.

1. **reactor**: This node specifies the reactor parameters such as its type, dimensions, operational mode, catalyst size, state (temperature and pressure) etc.

2. **simulation**: This node specifies the simulation parameters including those supplied to numerical solvers such as simulation time, stepping options for advancing (time for CSTR & batch, and distance for PFR), solver tolerances etc.

3. **phases**: OpenMKM expects 1 or 3 types of phases. For purely gas phase 
mechanism, a gas phase has to be specified. For heterogeneous reactions, two 
additional types of phases, one bulk solid phase and one or more surface phases 
have to be specified. The phases are actually defined in the Cantera input file, which is
supplied as the second argument. Since a cantera input XML file allows for defining 
arbitrary number of phases, the names of the phase definitions to be used in the 
simulation are specified in the *rctr.yaml*. The Cantera XML file can contain 
defitions of additional phases not specified in the *rctr.yaml* file. This way 
one could use a single XML file and just the phase definitions for different runs 
if required. In addition to the names of the phase definitions, this node is also 
used to specify the initial composition of the phases.

4. **inlet_gas**: This node is required only for CSTR and PFR models. These models require a continous input of feed with a flow rate and composition, which can be defined by this node.

Predefined templates are available for batch, CSTR, PFR, TPD and temperature 
profiled PFR reactor models in the *\<OpenMKM_ROOT\>/data/input_files/* folder. 
The users can quickly modify those template files rather than trying to write 
*rctr.yaml* from scratch. The *rctr_tmplt.yaml* file in the same folder 
contains full explanation of all the possible keywords given as comments next to 
each of the keywords.


## Cantera XML Input File.
The second argument to OpenMKM is the name of Cantera XML file specifiying the definitions of thermodynamics of species, kinetics of reactions, and phases, which can be thought as collection of species and reactions.

Cantera defines two format types  for its input called CTI and XML. Data defined in CTI format can be converted into XML format. The users are expected to work with CTI format, which is essentially a python code, and convert the CTI file into XML format using the 
supplied *\<OpenMKM\_ROOT\>/scripts/ctml_writer.py* script. Note that this script differs from the *ctml\_writer.py* supplied with Cantera. 

CTI and XML file formats are explained on Cantera website. Please read the [Cantera documentation](https://cantera.org/tutorials/input-files.html)  on the CTI and XML input file formats before reading further.

### Coverage dependent **lateral interactions** between surface species

One main addition to the CTI and XML formats done by us is to add 
specification for coverage dependent lateral interactions. 
Coverage effects need to be incorporated into the xml file supplied to OpenMKM. Since it is easy to work with CTI files, users have the option of specifying coverage effects in CTI file and then convert the CTI file into XML file with the python script *ctml_writer.py*.
To specifiy that a surface
has species with coverage dependent lateral interactions, in the CTI file, change the *interface* keyword to *interacting\_interface* and add *interactions="all"* in the arguments to *interacting\_interface*. 

Following is an example surface phase definition required by OpenMKM 

```python
interacting_interface(name='TERRACE',
                elements="H N Ru He",
                species="N2(S1)   N(S1)    H(S1)    NH3(S1)  NH2(S1)  NH(S1) RU(S1)",
                site_density=2.1671e-09,
                phases="gas BULK",
                reactions='all',
                interactions='all',
                initial_state=state(temperature=300.0, pressure=OneAtm))

```

The following is the original CTI defintion for the same phase without lateral interactions.

```python
interface(name='TERRACE',
          elements="H N Ru He",
          species="N2(S1)   N(S1)    H(S1)    NH3(S1)  NH2(S1)  NH(S1) RU(S1)",
          site_density=2.1671e-09,
          phases="gas BULK",
          reactions='all',
          initial_state=state(temperature=300.0, pressure=OneAtm))

```

Please note that *interacting\_interface* is superset of *interacting\_interface*. It means it is safe to use *interacting\_interface* for all surface phase whether lateral interactions are present or not. *At present, OpenMKM works only with surface phases defined with interacting\_interface keyword.*
For phases without any lateral interactions use the following surface phase definition:

```python
interacting_interface(name='TERRACE',
                      elements="H N Ru He",
                      species="N2(S1)   N(S1)    H(S1)    NH3(S1)  NH2(S1)  NH(S1) RU(S1)",
                      site_density=2.1671e-09,
                      phases="gas BULK",
                      reactions='all',
                      initial_state=state(temperature=300.0, pressure=OneAtm))

```

Now to specify the lateral interaction, use the *lateral\_interactions* keyword 
in the CTI file, which requires three keyword, *species*, -- species which have lateral interactions, *interaction\_matrix*, -- a matrix denoting the lateral interaction strengths, and *coverage\_thresholds*, -- coverage thresholds above which lateral interactions modifyAGibbs free energies of reactions.  

```python
lateral_interactions(
    species = 'N(S1) H(S1) NH3(S1) NH2(S1) NH(S1)',
    interaction_matrix = [[-47.0179, -17.7545, -25.1631, -20.7620, -48.7823],
                          [-17.7545,  -6.7043,  -9.5019,  -7.8400, -18.4208],
                          [-25.1631,  -9.5019, -13.4668, -11.1115, -26.1074],
                          [-20.762,   -7.8400, -11.1115,  -9.1681, -21.5412],
                          [-48.7823, -18.4208, -26.1074, -21.5412, -50.6129]],
    coverage_thresholds = [0, 0, 0, 0, 0])
```

### Chemkin Users 
Use
*\<CANTERA\_ROOT\>/interfaces/cython/cantera/ck2cti.py*, which parses gas.inp, surf.inp and thermdat files
to convert Chemkin files into the Cantera CTI input format. 
For more information, refer to 
[Cantera documentation](https://cantera.org/tutorials/input-files.html)  on input file format.

The chemkin input files are not sometimes parsed by ck2cti.py script. The troublesome file is often the surf.inp file due to bulk phase defintion. Remove the bulk phase defintion in surf.inp and retry. If it works, add the missing bulk phase definition directly in CTI file using the 
*stoichiometric\_solid* keyword. Example definition is given below.

```python
stoichiometric_solid(name='bulk',
                     elements="Ru",
                     species="RU(B)",
                     density=12.4,
                     initial_state=state(temperature=300.0, pressure=OneAtm))
```




