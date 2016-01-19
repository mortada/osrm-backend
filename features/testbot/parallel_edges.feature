@routing @testbot @parallel_arcs
Feature: Oneway Roundabout

    Background:
        Given the profile "testbot"

    Scenario: Simple Circle
        Given the node map
            |   |   | g |   |   |
            | a | b |   | d | f |
            |   |   | c |   |   |

        And the ways
            | nodes | oneway |
            | ab    | no     |
            | bcd   | yes    |
            | df    | no     |
            | dgb   | yes    |

        When I route I should get
            | waypoints | route     |
            | a,f       | ab,bcd,df |
            | f,a       | df,dgb,ab |
            | b,d       | bcd       |
            | d,b       | dgb       |

    Scenario: Parallel Geometry
        Given the node map
            | a | b | c | g | d | f |

        And the ways
            | nodes | oneway |
            | ab    | no     |
            | bcd   | yes    |
            | df    | no     |
            | dgb   | yes    |

        When I route I should get
            | waypoints | route     |
            | a,f       | ab,bcd,df |
            | f,a       | df,dgb,ab |
            | b,d       | bcd       |
            | d,b       | dgb       |

     Scenario: Parallel Geometry
        Given the node map
            | a | b |

        And the ways
            | nodes | oneway | highway |
            | ab    | yes    | primary |
            | ba    | yes    | tertiary |

        When I route I should get
            | waypoints | route   | time |
            | a,b       | ab      | 10s  |
            | b,a       | ba      | 30s  |

     Scenario: Parallel Geometry
        Given the node map
            | a | b |

        And the ways
            | nodes | oneway | highway  |
            | ba    | yes    | tertiary |
            | ab    | yes    | primary  |

        When I route I should get
            | waypoints | route   | time |
            | a,b       | ab      | 10s  |
            | b,a       | ba      | 30s  |
