FROM centos:6

ADD Recipe.deps /Recipe.deps
RUN bash -ex Recipe.deps 

ADD Recipe /Recipe
RUN bash -ex Recipe
