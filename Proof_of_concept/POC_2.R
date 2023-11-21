
  #**********************************************#
  #   ROBOTIC FLOWER FIELD - PROOF OF CONCEPT    #
  #                                              #
  #**********************************************#


## 0) load packages & set working directory ####
library(lubridate)
library(splines)
library(splines2)
library(emmeans)
library(lme4)
library(effects)
library(dplyr)
library(tidyverse)
# devtools::install_github("tomwenseleers/export")
library(export)
library(scales)
library(ggplot2)
library(ggthemes)
library(car)
library(stats)

# setwd("C:/Users/kamie/OneDrive - KU Leuven/Supporting")

## 1) load, inspect and prepare data ####

data = read.csv("S9_POC_data.csv")

head(data)
str(data)

# prepare parameters
data$date = as.Date(data$date)
data$time = hms(data$time)
data$exp_day = as.Date(data$exp_day)

data$exp_day = as.factor(data$exp_day)
data$color = as.factor(data$color)
data$replicate = as.factor(data$replicate)

data$flower = factor(data$flower, levels=paste0("flower", c(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20))) # flower ID
data$flower = droplevels(data$flower) # drop unused factor levels

data$column = factor(data$column)
data$row = factor (data$row)

## 2) creating new parameters ####

# create new parameters for data & time 
data$exp_day_time = ymd_hms(paste(data$exp_day, data$time))
data$day = day(data$exp_day_time)
data$hour = hour(data$time)

# create new parameter 'flower_no' with only the flower number
data$flower_no = factor(case_when(data$flower=="flower1" ~ "01 ",
                                  data$flower=="flower2" ~ "02 ",
                                  data$flower=="flower3" ~ "03 ",
                                  data$flower=="flower4" ~ "04 ",
                                  data$flower=="flower5" ~ "05 ",
                                  data$flower=="flower6" ~ "06 ",
                                  data$flower=="flower7" ~ "07 ",
                                  data$flower=="flower8" ~ "08 ",
                                  data$flower=="flower9" ~ "09 ",
                                  data$flower=="flower10" ~ "10 ",
                                  data$flower=="flower11" ~ "11 ",
                                  data$flower=="flower12" ~ "12 ",
                                  data$flower=="flower13" ~ "13 ",
                                  data$flower=="flower14" ~ "14 ",
                                  data$flower=="flower15" ~ "15 ",
                                  data$flower=="flower16" ~ "16 ",
                                  data$flower=="flower17" ~ "17 ",
                                  data$flower=="flower18" ~ "18 ",
                                  data$flower=="flower19" ~ "19 ",
                                  data$flower=="flower20" ~ "20 "))


# create new parameter 'concentration', storing data in hour blocks
data$concentration = factor(data$sugar_conc, 
                            levels=c("0.3", "0.6"), labels=c("low","high")) # nectar concentration (% biogluc)


#check new dataset
str(data)
head(data)

# remove duplicate lines in data (flowers can send data again when confirmation is not received)
duplicate_lines <- data %>% # find duplicates
  group_by(flower, exp_day, time) %>%
  filter(n() > 1) %>%
  arrange(flower, exp_day, time)

print(duplicate_lines)

data_filtered <- data %>% # keep only one of the duplicates
  group_by(flower, exp_day, time) %>%
  mutate(probing = round(mean(probing))) %>%
  distinct(flower, exp_day, time, .keep_all = TRUE)

data = data_filtered

#add observation level to dataset
data$obs=factor(1:nrow(data))


## 3) Heat map of visits per flower ####

# create new parameter 'exp_day_hour', storing data in hour blocks
step = "2 hours" # desired blocks to aggregate over
data$exp_day_hour = floor_date(data$exp_day_time, unit = step, # rounded down to X hour blocks
                               week_start = getOption("lubridate.week.start", 7))

# create data subset
data_visit_flower = data[,c("flower_no", "exp_day_hour", "probing", "concentration")]  %>%
  group_by(concentration) %>%
  complete(flower_no, exp_day_hour = seq(min(data$exp_day_hour), max(data$exp_day_hour), by=step),
           fill = list(probing = 0)) %>%
  group_by(flower_no, exp_day_hour,concentration) %>%
  count()

head(data_visit_flower)

# plot
mypal = colorRampPalette(c("mediumblue", "red2", "yellow"),interpolate="spline",bias=1)(n=1000)

pl1 = qplot(data=data_visit_flower, x=exp_day_hour, fill=n, y=flower_no, geom="tile") +
  scale_fill_gradientn("no. of visits", colors=mypal) +
  scale_x_datetime(labels = date_format("day %d - %Hh"),
                   date_breaks = "6 hours", minor_breaks = "2 hours", expand=c(0,0)) +
  scale_y_discrete(expand=c(0,0)) +
  theme(axis.text.x=element_text(angle=60, hjust=1)) +
  xlab("") + ylab("Flower") + # ggtitle("Heatmap flower visits") +
  facet_grid(data_visit_flower$concentration)
pl1

#graph2ppt(pl1, file="heatmap.pptx", width=5, height=5)
#ggsave(pl1, file="heatmap.pdf", width=5, height=5)


## 4) Model fitting ####

# prepare dataset with visit counts 

step = "1 hours" # desired blocks to aggregate over
data$step_of_day = floor_date(data$exp_day_time, unit = step, # rounded down to X time blocks
                              week_start = getOption("lubridate.week.start", 7))

model_data = data[,c("concentration", "step_of_day", "probing", "flower", "row", "column")] %>%
  group_by(flower,concentration,step_of_day,row, column) %>%
  count() %>%
  group_by(flower,row,column) %>%
  complete(concentration, step_of_day = seq(min(data$exp_day_hour)-60*60*6, max(data$exp_day_hour)+60*60*5, by=step), fill = list(probing = 0))%>%
  mutate_if(is.integer, ~replace(., is.na(.), 0))

model_data$exp_day = day(model_data$step_of_day)
model_data$exp_day_f = as.factor(model_data$exp_day)
model_data$hour_of_day = hour(model_data$step_of_day)
model_data$n = as.integer(model_data$n)

model_data$obs=factor(1:nrow(model_data))

str(model_data)
head(model_data)


# make model with different df for circadian spline

fit1 = glmer(n ~ (1|flower) + concentration * scale(exp_day) +
              mSpline(hour_of_day, df=3, Boundary.knots = c(0,24), periodic=TRUE) + # circadian rhythm, df = 3
              (1|obs), # observational level random factor
              family = poisson (link="log"), data = model_data,
              control = glmerControl(optimizer = "Nelder_Mead", optCtrl = list(maxfun=100000)))

fit2 = glmer(n ~ (1|flower) + concentration * scale(exp_day) +
              mSpline(hour_of_day, df=4, Boundary.knots = c(0,24), periodic=TRUE) + # circadian rhythm, df = 4
              (1|obs), # observational level random factor
              family = poisson (link="log"), data = model_data,
              control = glmerControl(optimizer = "Nelder_Mead", optCtrl = list(maxfun=100000)))

fit3 = glmer(n ~ (1|flower) + concentration * scale(exp_day) +
              mSpline(hour_of_day, df=5, Boundary.knots = c(0,24), periodic=TRUE) + # circadian rhythm, df = 5
              (1|obs), # observational level random factor
              family = poisson (link="log"), data = model_data,
              control = glmerControl(optimizer = "Nelder_Mead", optCtrl = list(maxfun=100000)))

fit4 = glmer(n ~ (1|flower) + concentration * scale(exp_day) +
              mSpline(hour_of_day, df=6, Boundary.knots = c(0,24), periodic=TRUE) + # circadian rhythm, df = 6
              (1|obs), # observational level random factor
              family = poisson (link="log"), data = model_data,
              control = glmerControl(optimizer = "Nelder_Mead", optCtrl = list(maxfun=100000)))

fit5 = glmer(n ~ (1|flower) + concentration * scale(exp_day) +
              mSpline(hour_of_day, df=7, Boundary.knots = c(0,24), periodic=TRUE) + # circadian rhythm, df = 7
              (1|obs), # observational level random factor
              family = poisson (link="log"), data = model_data,
              control = glmerControl(optimizer = "Nelder_Mead", optCtrl = list(maxfun=100000)))

fit6 = glmer(n ~ (1|flower) + concentration * scale(exp_day) +
              mSpline(hour_of_day, df=8, Boundary.knots = c(0,24), periodic=TRUE) + # circadian rhythm, df = 8
              (1|obs), # observational level random factor
              family = poisson (link="log"), data = model_data,
              control = glmerControl(optimizer = "Nelder_Mead", optCtrl = list(maxfun=100000)))

fit7 = glmer(n ~ (1|flower) + concentration * ns(exp_day, df=2) + # exp_day with natural spline
              mSpline(hour_of_day, df=3, Boundary.knots = c(0,24), periodic=TRUE) + # circadian rhythm, df = 3
              (1|obs), # observational level random factor
              family = poisson (link="log"), data = model_data,
              control = glmerControl(optimizer = "Nelder_Mead", optCtrl = list(maxfun=100000)))

fit8 = glmer(n ~ (1|flower) + concentration * ns(exp_day, df=2) + # exp_day with natural spline
              mSpline(hour_of_day, df=4, Boundary.knots = c(0,24), periodic=TRUE) + # circadian rhythm, df = 4
              (1|obs), # observational level random factor
              family = poisson (link="log"), data = model_data,
              control = glmerControl(optimizer = "Nelder_Mead", optCtrl = list(maxfun=100000)))

fit9 = glmer(n ~ (1|flower) + concentration * ns(exp_day, df=2) + # exp_day with natural spline
              mSpline(hour_of_day, df=5, Boundary.knots = c(0,24), periodic=TRUE) + # circadian rhythm, df = 5
              (1|obs), # observational level random factor
              family = poisson (link="log"), data = model_data,
              control = glmerControl(optimizer = "Nelder_Mead", optCtrl = list(maxfun=100000)))

fit10 = glmer(n ~ (1|flower) + concentration * ns(exp_day, df=2) + # exp_day with natural spline
              mSpline(hour_of_day, df=6, Boundary.knots = c(0,24), periodic=TRUE) + # circadian rhythm, df = 6
              (1|obs), # observational level random factor
              family = poisson (link="log"), data = model_data,
              control = glmerControl(optimizer = "Nelder_Mead", optCtrl = list(maxfun=100000)))

fit11 = glmer(n ~ (1|flower) + concentration * ns(exp_day, df=2) + # exp_day with natural spline
              mSpline(hour_of_day, df=7, Boundary.knots = c(0,24), periodic=TRUE) + # circadian rhythm, df = 7
              (1|obs), # observational level random factor
              family = poisson (link="log"), data = model_data,
              control = glmerControl(optimizer = "Nelder_Mead", optCtrl = list(maxfun=100000)))

fit12 = glmer(n ~ (1|flower) + concentration * ns(exp_day, df=2) + # exp_day with natural spline
              mSpline(hour_of_day, df=8, Boundary.knots = c(0,24), periodic=TRUE) + # circadian rhythm, df = 8
              (1|obs), # observational level random factor
              family = poisson (link="log"), data = model_data,
              control = glmerControl(optimizer = "Nelder_Mead", optCtrl = list(maxfun=100000)))


BIC(fit1, fit2, fit3, fit4, fit5, fit6, fit7, fit8, fit9, fit10, fit11, fit12) #fit5 has best BIC

bestfit = fit5


## 5) Model conclusions, effect plots & contrasts ####

summary(bestfit)
a <- Anova(bestfit, type=3)
a

#write.table(a, file = "anova.csv", sep = ",", quote = T, row.names = T) #export table

# effect plots
plot(Effect(c("concentration","exp_day"), bestfit, confidence.level=0.95), type ="response")
plot(Effect(c("hour_of_day"), bestfit, confidence.level=0.95, 
            xlevels = list(hour_of_day=seq(0,24, by=0.1))), type = "response")
plot(allEffects(bestfit, xlevels = list(exp_day=c(1,2,3,4,5),
                                          hour_of_day=seq(0,24, by=0.1))), type ="response")


df = data.frame(Effect(c("concentration", "exp_day", "hour_of_day"), 
                       mod = bestfit, 
                       xlevels = list(exp_day=c(1,2,3,4,5),
                                 hour_of_day=seq(0,24,by=0.1)),
                       type = "response"))
df$year = "2022"
df$month = "October"
df$date = with(df, ymd_h(paste(year, month, exp_day, hour_of_day, sep= ' ')))
model_data$year = "2022"
model_data$month = "October"
model_data$date = with(model_data, ymd_h(paste(year, month, exp_day, hour_of_day, sep= ' ')))

pl2 = ggplot(df, aes(x=date, y=fit, color=concentration, fill=concentration)) +
  geom_point(data = model_data, aes(x=date, y=n), alpha=I(0.5), pch=16, size=0.7) +
  theme_few() +
  geom_ribbon(alpha=0.4, aes(color=NULL, ymin=lower, ymax=upper)) + # confidence intervals
  geom_line(size=1) + # line plot with predicted regressions
  #theme(legend.position = "FALSE") +
  xlab("Experimental day") + ylab("No. of visits per flower per hour") + # ggtitle("Effectplot (concentration * experimental day)") +
  scale_x_datetime(labels = date_format("%d")) +
  coord_cartesian(expand=FALSE, xlim=c(ymd_h(paste("2022", "October", "1", "0", sep= ' ')),NA), ylim=c(10,120)) +
  scale_colour_manual(values=c("mediumblue","red2")) +
  scale_fill_manual(values=c("mediumblue","red2")) +
  scale_y_continuous(trans="log10")
pl2

#graph2ppt(pl, file="effect plot.pptx", width=6, height=5)
#ggsave(pl, file="effect plot.pdf", width=6, height=5)


# contrasts in visitation rate through time (emmeans)
formula(bestfit)
tab1 = contrast(emmeans(bestfit, ~ concentration | exp_day, type = "response", 
                at = list(exp_day = 1:5)), 
                method = "revpairwise", adjust = "Tukey")
tab1
#write.table(tab1, file = "contrasts_emmeans.csv", sep = ",", quote = T, row.names = T) #export table


# contrasts in emtrends, ie in rate of increase in visitation rate of low vs high conc
tab2 = contrast(emtrends(bestfit, ~ concentration, type = "link",
                var = "exp_day",
                at = list(exp_day = 1:5)), 
                method = "revpairwise", adjust = "Tukey")
tab2
#write.table(tab2, file = "contrasts_emtrends.csv", sep = ",", quote = T, row.names = T) #export table
