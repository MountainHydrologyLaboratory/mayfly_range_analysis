library(readr)
SONAR01 <- read_csv("~/Desktop/Mayfly/SONAR01.csv", 
                    col_names = FALSE)
SONAR01<-SONAR01[2:894,]
View(SONAR01)
volt<-SONAR01$X2
range<-SONAR01$X3
td=seq(as.POSIXct("2017/11/29 12:55"), as.POSIXct("2017-12-02 15:15"), by=300)
plot(td,range,type="l")
summary(range)
boxplot(range)
sd(range)
mean(range)
dnorm(range)
hist(range)
dist<-as.ts.defautl(range,start = 2017/11/29 12:55, end = 2017-12-02 15:15, frequency(1))
plot(dist)
decompose(dist)
require(ggplot2)
ggplot(data=SONAR01, aes(x=td,y=range))+geom_point()
